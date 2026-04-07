#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
End-to-end demo: real **libul_ecat** master (AF_PACKET) + **ecat_slave_sim.py** on a veth pair.

This is **not** the TCP harness (`ul_ecat_slave_harness`); that path only exercises `libul_ecat_slave`
with a Python controller. Here the **C master** sends real EtherCAT frames on L2.

Requires **root** (or CAP_NET_RAW). Creates `ul_ecat_e2e0` / `ul_ecat_e2e1`, tears them down on exit.

Usage::

  sudo env UL_ECAT_LIB=$PWD/build/libul_ecat.so python3 scripts/run_e2e_l2_scan.py

Verbose master scan logs: ``UL_ECAT_VERBOSE=1`` (set by default in this script).
"""
from __future__ import annotations

import ctypes
import os
import subprocess
import sys
import time
from pathlib import Path

VETH0 = "ul_ecat_e2e0"
VETH1 = "ul_ecat_e2e1"

EXP_VENDOR = 0x00000002
EXP_PRODUCT = 0x1C213052


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _read_mac(ifname: str) -> list[int]:
    path = f"/sys/class/net/{ifname}/address"
    with open(path, encoding="utf-8") as f:
        line = f.readline().strip()
    parts = line.split(":")
    return [int(x, 16) for x in parts]


def _ip(*args: str, check: bool = True) -> None:
    subprocess.run(["ip", *args], check=check)


def _cleanup_veth() -> None:
    _ip("link", "del", VETH0, check=False)


def main() -> int:
    if os.geteuid() != 0:
        print("This E2E test must run as root (raw sockets + veth). Example:", file=sys.stderr)
        print(
            f"  sudo env UL_ECAT_LIB={_repo_root() / 'build' / 'libul_ecat.so'} "
            f"{sys.executable} {__file__}",
            file=sys.stderr,
        )
        return 1

    root = _repo_root()
    so = os.environ.get("UL_ECAT_LIB") or str(root / "build" / "libul_ecat.so")
    if not os.path.isfile(so):
        print(f"Shared library not found: {so}", file=sys.stderr)
        print("Build with: cmake -S . -B build -DUL_ECAT_BUILD_SHARED=ON && cmake --build build", file=sys.stderr)
        return 1

    os.environ.setdefault("UL_ECAT_VERBOSE", "1")

    print("=== ul_ecat E2E: L2 master (libul_ecat.so) + ecat_slave_sim (Python) ===", flush=True)
    print(f"Library: {so}", flush=True)

    _cleanup_veth()
    try:
        _ip("link", "add", VETH0, "type", "veth", "peer", "name", VETH1)
        _ip("link", "set", VETH0, "up")
        _ip("link", "set", VETH1, "up")
    except subprocess.CalledProcessError as exc:
        print(f"Failed to create veth pair: {exc}", file=sys.stderr)
        return 1

    sim_py = root / "scripts" / "ecat_slave_sim.py"
    # Inherit stdout/stderr so slave logs interleave with master (UL_ECAT_VERBOSE) lines.
    sim = subprocess.Popen([sys.executable, str(sim_py), "-v", VETH1])
    time.sleep(0.35)
    print(f"--- master iface={VETH0} (scan) ---", flush=True)

    exit_code = 1
    try:
        lib = ctypes.CDLL(so)

        class MasterSettings(ctypes.Structure):
            _fields_ = [
                ("iface_name", ctypes.c_char_p),
                ("dst_mac", ctypes.c_uint8 * 6),
                ("src_mac", ctypes.c_uint8 * 6),
                ("rt_priority", ctypes.c_int),
                ("dc_enable", ctypes.c_int),
                ("dc_sync0_cycle", ctypes.c_uint32),
            ]

        lib.ul_ecat_master_init.argtypes = [ctypes.POINTER(MasterSettings)]
        lib.ul_ecat_master_init.restype = ctypes.c_int
        lib.ul_ecat_master_shutdown.restype = ctypes.c_int
        lib.ul_ecat_scan_network.restype = ctypes.c_int

        mac = _read_mac(VETH0)
        cfg = MasterSettings()
        cfg.iface_name = VETH0.encode("utf-8")
        cfg.dst_mac = (ctypes.c_uint8 * 6)(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)
        cfg.src_mac = (ctypes.c_uint8 * 6)(*mac)
        cfg.rt_priority = 0
        cfg.dc_enable = 0
        cfg.dc_sync0_cycle = 1_000_000

        if lib.ul_ecat_master_init(ctypes.byref(cfg)) != 0:
            print("ul_ecat_master_init failed", file=sys.stderr)
            return 1
        try:
            rc = lib.ul_ecat_scan_network()
            if rc != 0:
                print(f"ul_ecat_scan_network returned {rc}", file=sys.stderr)
        finally:
            lib.ul_ecat_master_shutdown()

        lib.ul_ecat_get_master_slaves.restype = ctypes.c_void_p
        dbp = lib.ul_ecat_get_master_slaves()

        class Slave(ctypes.Structure):
            _fields_ = [
                ("station_address", ctypes.c_uint16),
                ("state", ctypes.c_int),
                ("vendor_id", ctypes.c_uint32),
                ("product_code", ctypes.c_uint32),
                ("revision_no", ctypes.c_uint32),
                ("serial_no", ctypes.c_uint32),
                ("device_name", ctypes.c_char * 128),
            ]

        class SlavesDb(ctypes.Structure):
            _fields_ = [("slaves", Slave * 16), ("slave_count", ctypes.c_int)]

        db = ctypes.cast(dbp, ctypes.POINTER(SlavesDb)).contents
        print("--- result ---", flush=True)
        print(f"slave_count={db.slave_count}", flush=True)
        ok = db.slave_count >= 1 and db.slaves[0].vendor_id == EXP_VENDOR and db.slaves[0].product_code == EXP_PRODUCT
        if db.slave_count >= 1:
            s0 = db.slaves[0]
            print(
                f"Slave[0]: station=0x{s0.station_address:04X} vend=0x{s0.vendor_id:08X} "
                f"prod=0x{s0.product_code:08X} rev=0x{s0.revision_no:08X} ser=0x{s0.serial_no:08X}",
                flush=True,
            )
        if ok:
            print("E2E scan: PASS (identity matches EL7201-like simulator defaults)", flush=True)
        else:
            print("E2E scan: FAIL (expected at least one slave with Beckhoff-like vendor/product)", flush=True)
        exit_code = 0 if ok else 1
    finally:
        if sim.poll() is None:
            sim.terminate()
            try:
                sim.wait(timeout=3)
            except subprocess.TimeoutExpired:
                sim.kill()
        _cleanup_veth()

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
