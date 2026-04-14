#!/usr/bin/env python3
"""
EtherCAT master CLI (Linux libul_ecat.so via ctypes).

- Interactive REPL (TTY, no args): bluetoothctl-style session — ``interface``, ``scan``,
  ``list``, ``read``, ``write``, ``help``, ``quit``.
- ``-c/--command "cmd1; cmd2"``: same commands, non-interactive (e.g. CI).
- Legacy: ``%(prog)s <iface> <command> [args...]`` forwards to ``ul_ecat_app_execute`` (C).
- ``slave-emulator``: TCP harness + Python controller simulator (unchanged).

Environment: ``UL_ECAT_LIB``, ``UL_ECAT_SLAVE_HARNESS`` (see slave-emulator help).
"""
from __future__ import annotations

import argparse
import cmd
import ctypes
import os
import shlex
import subprocess
import sys
import time
from typing import List, Optional, Sequence, TextIO, Tuple


def _detect_preempt_rt_linux() -> Tuple[bool, str]:
    """
    Best-effort PREEMPT_RT detection (Linux only).
    Returns (True, reason) if the running kernel likely supports RT scheduling well.
    """
    if sys.platform != "linux":
        return False, ""
    try:
        rt_sys = "/sys/kernel/realtime"
        if os.path.isfile(rt_sys):
            with open(rt_sys, encoding="utf-8") as f:
                if f.read().strip() == "1":
                    return True, "sys/kernel/realtime=1"
    except OSError:
        pass
    try:
        with open("/proc/version", encoding="utf-8", errors="replace") as f:
            ver = f.read()
        if "PREEMPT_RT" in ver or "PREEMPT RT" in ver:
            return True, "PREEMPT_RT in /proc/version"
    except OSError:
        pass
    try:
        rel = os.uname().release
    except AttributeError:
        rel = ""
    if rel and ("-rt" in rel or rel.endswith("rt") or "preempt_rt" in rel.lower()):
        return True, f"release string ({rel})"
    return False, ""


def _kernel_timing_banner_line() -> str:
    """One line at REPL / -c startup: RT kernel status (Linux) or platform note."""
    if sys.platform != "linux":
        return (
            "[kernel] Not Linux — PREEMPT_RT hints apply only on Linux; "
            "timing behaviour depends on the host OS."
        )
    ok, reason = _detect_preempt_rt_linux()
    if ok:
        return (
            "[RT] PREEMPT_RT kernel detected "
            f"({reason}) — master timing/SCHED_FIFO can be more precise."
        )
    return (
        "[kernel] PREEMPT_RT not detected on this Linux kernel — expect higher jitter; "
        "a real-time (PREEMPT_RT) kernel improves cycle precision for the master."
    )


def _find_library() -> str:
    env = os.environ.get("UL_ECAT_LIB")
    if env and os.path.isfile(env):
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, ".."))
    cand = os.path.join(root, "build", "libul_ecat.so")
    if os.path.isfile(cand):
        return cand
    return "libul_ecat.so"


def _find_harness() -> str | None:
    env = os.environ.get("UL_ECAT_SLAVE_HARNESS")
    if env and os.path.isfile(env):
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, ".."))
    cand = os.path.join(root, "build", "ul_ecat_slave_harness")
    if os.path.isfile(cand):
        return cand
    return None


# --- ctypes: master session + structs (from ecat_ctypes) ---

from ecat_ctypes import (
    UlEcatMasterSettings,
    UlEcatSlaveT,
    UlEcatMasterSlavesT,
    bind_master_api,
)


_SLAVE_STATE_NAMES = {
    1: "INIT",
    2: "PREOP",
    3: "BOOT",
    4: "SAFEOP",
    8: "OP",
    16: "ERROR",
}


class MasterSession:
    """Holds CDLL and optional open master (init/shutdown)."""

    def __init__(self, libpath: str) -> None:
        self.libpath = libpath
        self.lib = ctypes.CDLL(libpath)
        self._iface_storage: Optional[bytes] = None
        self.opened = False
        bind_master_api(self.lib)

    def init(self, iface: str) -> int:
        if self.opened:
            self.lib.ul_ecat_master_shutdown()
            self.opened = False
        self._iface_storage = iface.encode("utf-8")
        cfg = UlEcatMasterSettings()
        cfg.iface_name = ctypes.c_char_p(self._iface_storage)
        cfg.dst_mac = (ctypes.c_ubyte * 6)()
        cfg.src_mac = (ctypes.c_ubyte * 6)(0x02, 0x00, 0x00, 0x00, 0x00, 0x01)
        self.lib.ul_ecat_mac_broadcast(ctypes.cast(cfg.dst_mac, ctypes.POINTER(ctypes.c_ubyte)))
        cfg.rt_priority = 50
        cfg.dc_enable = 0
        cfg.dc_sync0_cycle = 1000000
        r = int(self.lib.ul_ecat_master_init(ctypes.byref(cfg)))
        self.opened = r == 0
        return r

    def shutdown(self) -> None:
        if self.opened:
            self.lib.ul_ecat_master_shutdown()
            self.opened = False

    def require_open(self, out: TextIO) -> bool:
        if not self.opened:
            out.write("No interface: use 'interface <iface>' first.\n")
            return False
        return True


def _run_master_ctypes_argv(argv: Sequence[str]) -> int:
    libpath = _find_library()
    try:
        lib = ctypes.CDLL(libpath)
    except OSError as exc:
        print(f"Failed to load {libpath}: {exc}", file=sys.stderr)
        print("Build with -DUL_ECAT_BUILD_SHARED=ON and set UL_ECAT_LIB if needed.", file=sys.stderr)
        return 1

    lib.ul_ecat_app_execute.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
    lib.ul_ecat_app_execute.restype = ctypes.c_int

    argc = len(argv)
    bufs = [ctypes.create_string_buffer(s.encode("utf-8")) for s in argv]
    c_argv = (ctypes.c_char_p * argc)()
    for i in range(argc):
        c_argv[i] = ctypes.cast(bufs[i], ctypes.c_char_p)

    return int(lib.ul_ecat_app_execute(argc, c_argv))


def _tokenize_line(line: str) -> List[str]:
    line = line.strip()
    if not line:
        return []
    try:
        return shlex.split(line, comments=False)
    except ValueError as exc:
        raise ValueError(str(exc)) from exc


def _eprint_raw_socket_help(err: TextIO) -> None:
    """Linux AF_PACKET requires CAP_NET_RAW (not a bug in ul_ecat)."""
    err.write(
        "Linux raw Ethernet (AF_PACKET) needs CAP_NET_RAW. Typical fixes:\n"
        "  • Run this tool with sudo, e.g.  sudo python3 scripts/ul_ecat_tool.py\n"
        "  • Or grant the capability to your Python binary (advanced):\n"
        "      sudo setcap cap_net_raw+eip \"$(readlink -f \"$(command -v python3)\")\"\n"
        "    (only if you accept the security trade-off.)\n"
        "If errno was ENOENT or ENODEV, the interface name may be wrong (ip link).\n"
    )


_RESERVED_IFACE_NAMES = frozenset(
    {"scan", "list", "read", "write", "help", "quit", "exit", "interface", "slave-emulator"}
)


def _dispatch_tokens(sess: MasterSession, tokens: Sequence[str], out: TextIO, err: TextIO) -> int:
    if not tokens:
        return 0
    cmd = tokens[0].lower()
    args = tokens[1:]

    if cmd == "quit":
        return 2
    if cmd == "help":
        out.write(
            "Commands: interface <iface> | scan | list | read <adp_hex> <ado_hex> <len> | "
            "write <adp_hex> <ado_hex> <val_hex> [size] | help | quit\n"
            "Opening an interface uses Linux raw sockets — run with sudo unless cap_net_raw is set.\n"
        )
        return 0
    if cmd == "interface":
        if len(args) < 1:
            err.write("usage: interface <iface>\n")
            return 1
        ifname = args[0]
        if ifname.lower() in _RESERVED_IFACE_NAMES:
            err.write(
                f"'{ifname}' looks like a command name, not a network interface. "
                "Use e.g.  interface eth0   then   scan\n"
            )
            return 1
        r = sess.init(ifname)
        if r != 0:
            err.write(f"ul_ecat_master_init failed ({r}).\n")
            _eprint_raw_socket_help(err)
            return 1
        out.write(f"Interface '{ifname}' opened.\n")
        return 0
    if cmd == "scan":
        if not sess.require_open(out):
            return 1
        r = int(sess.lib.ul_ecat_scan_network())
        if r != 0:
            err.write(f"scan failed ({r}).\n")
            return 1
        out.write("Scan done.\n")
        return 0
    if cmd == "list":
        if not sess.require_open(out):
            return 1
        p = sess.lib.ul_ecat_get_master_slaves()
        if not p:
            err.write("ul_ecat_get_master_slaves returned NULL.\n")
            return 1
        db = p.contents
        n = int(db.slave_count)
        if n == 0:
            out.write("No slaves in table (run 'scan' first).\n")
            return 0
        out.write(f"{'#':<4} {'Station':<8} {'State':<8} {'Vendor':<10} {'Product':<10} {'Rev':<10} {'Serial':<10} Name\n")
        for i in range(n):
            s = db.slaves[i]
            st = _SLAVE_STATE_NAMES.get(int(s.state), str(int(s.state)))
            name = s.device_name.split(b"\0", 1)[0].decode("utf-8", errors="replace")
            out.write(
                f"{i:<4} 0x{s.station_address:04X}   {st:<8} 0x{s.vendor_id:08X} "
                f"0x{s.product_code:08X} 0x{s.revision_no:08X} 0x{s.serial_no:08X} {name}\n"
            )
        return 0
    if cmd == "read":
        if len(args) < 3:
            err.write("usage: read <adp_hex> <ado_hex> <len_dec>\n")
            return 1
        if not sess.require_open(out):
            return 1
        adp = int(args[0], 16)
        ado = int(args[1], 16)
        ln = int(args[2], 0)
        if ln <= 0 or ln > 512:
            err.write("len must be 1..512\n")
            return 1
        buf = (ctypes.c_uint8 * ln)()
        n = int(sess.lib.ul_ecat_fprd_sync(ctypes.c_uint16(adp), ctypes.c_uint16(ado), ctypes.c_uint16(ln), buf, ctypes.c_size_t(ln), 2000))
        if n < 0:
            err.write("FPRD failed (timeout or WKC=0).\n")
            return 1
        for i in range(n):
            out.write(f"{buf[i]:02X} ")
        out.write("\n")
        return 0
    if cmd == "write":
        if len(args) < 3:
            err.write("usage: write <adp_hex> <ado_hex> <val_hex> [size]\n")
            return 1
        if not sess.require_open(out):
            return 1
        adp = int(args[0], 16)
        ado = int(args[1], 16)
        val = int(args[2], 16)
        sz = 4
        if len(args) >= 4:
            sz = int(args[3], 0)
        if sz < 1 or sz > 4:
            err.write("size must be 1..4\n")
            return 1
        v = ctypes.c_uint32(val)
        r = int(sess.lib.ul_ecat_fpwr_sync(ctypes.c_uint16(adp), ctypes.c_uint16(ado), ctypes.byref(v), ctypes.c_uint16(sz), 2000))
        if r != 0:
            err.write("FPWR failed (timeout or WKC=0).\n")
            return 1
        out.write("OK\n")
        return 0

    err.write(f"Unknown command: {cmd}\n")
    return 1


def _run_command_batch(sess: MasterSession, script: str, out: TextIO, err: TextIO) -> int:
    rc = 0
    for part in script.split(";"):
        part = part.strip()
        if not part:
            continue
        try:
            tokens = _tokenize_line(part)
        except ValueError as exc:
            err.write(f"{exc}\n")
            return 1
        x = _dispatch_tokens(sess, tokens, out, err)
        if x == 2:
            return rc
        if x != 0:
            rc = x
    return rc


def _repl_intro_text() -> str:
    return (
        _kernel_timing_banner_line()
        + "\n\nEtherCAT master (raw L2). Raw sockets need sudo or cap_net_raw — see 'help'.\n"
        "Type 'help' for commands, 'quit' to exit."
    )


class EcatCmdLoop(cmd.Cmd):
    prompt = "ul-ecat> "
    intro = ""  # set in main() via _repl_intro_text()

    def __init__(self, sess: MasterSession, stdin: Optional[TextIO] = None, stdout: Optional[TextIO] = None) -> None:
        super().__init__(stdin=stdin, stdout=stdout)
        self.sess = sess
        self._rc = 0
        # cmd.Cmd sets stdin/stdout but not stderr (Python stdlib)
        self.stderr = sys.stderr

    def emptyline(self) -> bool:
        return False

    def do_help(self, arg: str) -> None:
        if arg.strip():
            self.stdout.write("Use 'help' with no arguments for the command list.\n")
            return
        _dispatch_tokens(self.sess, ["help"], self.stdout, self.stderr)

    def do_quit(self, _: str) -> bool:
        self.sess.shutdown()
        return True

    def do_exit(self, _: str) -> bool:
        return self.do_quit("")

    def default(self, line: str) -> None:
        try:
            tokens = _tokenize_line(line)
        except ValueError as exc:
            self.stdout.write(f"{exc}\n")
            self._rc = 1
            return
        x = _dispatch_tokens(self.sess, tokens, self.stdout, self.stderr)
        if x != 0:
            self._rc = x

    def do_EOF(self, _: str) -> bool:
        self.stdout.write("\n")
        return self.do_quit("")


def _is_legacy_master_argv(argv: List[str]) -> bool:
    if len(argv) < 3:
        return False
    a1 = argv[1]
    if a1.startswith("-"):
        return False
    if a1 in ("slave-emulator", "slave_emulator"):
        return False
    return True


def _run_slave_emulator(argv: List[str]) -> int:
    p = argparse.ArgumentParser(prog="ul_ecat_tool.py slave-emulator", description=__doc__)
    p.add_argument("-p", "--port", type=int, default=9234, help="TCP port for harness (default 9234)")
    p.add_argument(
        "--no-scan",
        action="store_true",
        help="Only start harness and wait; do not run the controller simulator",
    )
    args = p.parse_args(argv)

    harness = _find_harness()
    if not harness:
        print(
            "ul_ecat_slave_harness not found. Build with -DUL_ECAT_BUILD_SLAVE=ON "
            "or set UL_ECAT_SLAVE_HARNESS.",
            file=sys.stderr,
        )
        return 1

    proc = subprocess.Popen(
        [harness, "-p", str(args.port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    time.sleep(0.2)
    if proc.poll() is not None:
        err = proc.stderr.read().decode("utf-8", errors="replace") if proc.stderr else ""
        print(f"harness exited early: {err}", file=sys.stderr)
        return 1

    if args.no_scan:
        print(f"Harness listening on 127.0.0.1:{args.port} (pid {proc.pid}). Ctrl+C to stop.", flush=True)
        try:
            proc.wait()
        except KeyboardInterrupt:
            proc.terminate()
            proc.wait(timeout=5)
        return 0

    here = os.path.dirname(os.path.abspath(__file__))
    if here not in sys.path:
        sys.path.insert(0, here)
    try:
        from ethercat_controller_sim import run_identity_scan
    except ImportError as exc:
        print(f"Could not import ethercat_controller_sim: {exc}", file=sys.stderr)
        proc.terminate()
        return 1

    try:
        r = run_identity_scan("127.0.0.1", args.port)
    except (OSError, RuntimeError, ValueError, ConnectionError) as exc:
        print(f"Scan failed: {exc}", file=sys.stderr)
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        return 1
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()

    print(
        f"OK: vendor=0x{r.vendor_id:08X} product=0x{r.product_code:08X} "
        f"rev=0x{r.revision:08X} serial=0x{r.serial:08X}"
    )
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    argv = list(sys.argv if argv is None else argv)
    prog = os.path.basename(argv[0]) if argv else "ul_ecat_tool.py"

    if len(argv) >= 2 and argv[1] in ("slave-emulator", "slave_emulator"):
        return _run_slave_emulator(argv[2:])

    ap = argparse.ArgumentParser(
        prog=prog,
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "-c",
        "--command",
        metavar="SCRIPT",
        help='Non-interactive: run semicolon-separated commands (e.g. \'interface eth0; scan; list\')',
    )
    args, rest = ap.parse_known_args(argv[1:])

    if _is_legacy_master_argv([argv[0]] + rest):
        return _run_master_ctypes_argv([argv[0]] + rest)

    libpath = _find_library()
    try:
        sess = MasterSession(libpath)
    except OSError as exc:
        print(f"Failed to load {libpath}: {exc}", file=sys.stderr)
        print("Build with -DUL_ECAT_BUILD_SHARED=ON and set UL_ECAT_LIB if needed.", file=sys.stderr)
        return 1

    if args.command is not None:
        print(_kernel_timing_banner_line(), flush=True)
        rc = _run_command_batch(sess, args.command, sys.stdout, sys.stderr)
        sess.shutdown()
        return rc

    if not sys.stdin.isatty():
        ap.print_help()
        print("\nNon-interactive mode requires -c SCRIPT or legacy: <iface> <command> ...", file=sys.stderr)
        sess.shutdown()
        return 1

    try:
        loop = EcatCmdLoop(sess)
        loop.intro = _repl_intro_text()
        loop.cmdloop()
        return loop._rc
    finally:
        sess.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
