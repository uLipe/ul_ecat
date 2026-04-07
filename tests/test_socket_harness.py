"""
Loopback TCP harness + Python EtherCAT controller simulator (no raw sockets, no root).
"""
from __future__ import annotations

import os
import socket
import subprocess
import sys
import time

import pytest

EXP_VENDOR = 0x00000002
EXP_PRODUCT = 0x1C213052


def _root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _harness_exe() -> str:
    return os.path.join(_root(), "build", "ul_ecat_slave_harness")


def _pick_loopback_port() -> int:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def test_harness_identity_scan_matches_generated_tables():
    exe = _harness_exe()
    if not os.path.isfile(exe):
        pytest.skip("ul_ecat_slave_harness not built (enable UL_ECAT_BUILD_SLAVE)")

    port = _pick_loopback_port()
    proc = subprocess.Popen(
        [exe, "-p", str(port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.15)
    try:
        scripts = os.path.join(_root(), "scripts")
        if scripts not in sys.path:
            sys.path.insert(0, scripts)
        from ethercat_controller_sim import run_identity_scan

        r = run_identity_scan("127.0.0.1", port)
        assert r.vendor_id == EXP_VENDOR
        assert r.product_code == EXP_PRODUCT
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
