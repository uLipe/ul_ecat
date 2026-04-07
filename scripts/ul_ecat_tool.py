#!/usr/bin/env python3
"""
CLI for ul_ecat:

- Default: load libul_ecat.so and call ul_ecat_app_execute (same argv as the C tool).
  Requires CAP_NET_RAW for raw sockets used by the library.

- ``slave-emulator``: start the TCP ``ul_ecat_slave_harness`` and run the Python EtherCAT
  controller simulator against it (loopback, no special privileges).

Environment:

- ``UL_ECAT_LIB`` — path to libul_ecat.so for the default mode.
- ``UL_ECAT_SLAVE_HARNESS`` — path to ``ul_ecat_slave_harness`` for ``slave-emulator``.
"""
from __future__ import annotations

import argparse
import ctypes
import os
import subprocess
import sys
import time


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


def _run_master_ctypes(argv: list[str]) -> int:
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


def _run_slave_emulator(argv: list[str]) -> int:
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


def main() -> int:
    if len(sys.argv) >= 2 and sys.argv[1] in ("slave-emulator", "slave_emulator"):
        return _run_slave_emulator(sys.argv[2:])
    return _run_master_ctypes(sys.argv)


if __name__ == "__main__":
    raise SystemExit(main())
