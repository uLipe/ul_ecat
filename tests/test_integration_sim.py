"""
Optional integration test: veth + Python slave sim + ctypes scan (root / CAP_NET_RAW).

Set UL_ECAT_INTEGRATION=1 and run pytest as root, or skip.
"""
from __future__ import annotations

import ctypes
import os
import subprocess
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")), "scripts"))
from ecat_wire import DEFAULT_VENDOR as EXP_VENDOR, DEFAULT_PRODUCT as EXP_PRODUCT

VETH0 = "ul_ecat_t0"
VETH1 = "ul_ecat_t1"


def _root() -> bool:
    return os.geteuid() == 0


def _integration_enabled() -> bool:
    return os.environ.get("UL_ECAT_INTEGRATION", "") == "1"


def _lib_path() -> str:
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return os.path.join(root, "build", "libul_ecat.so")


def _read_mac(ifname: str) -> bytes:
    path = f"/sys/class/net/{ifname}/address"
    with open(path, encoding="utf-8") as f:
        line = f.readline().strip()
    parts = line.split(":")
    return bytes(int(x, 16) for x in parts)


@pytest.mark.integration
@pytest.fixture(scope="module")
def veth_up():
    if not _integration_enabled() or not _root():
        pytest.skip("Need UL_ECAT_INTEGRATION=1 and root")
    subprocess.run(
        ["ip", "link", "add", VETH0, "type", "veth", "peer", "name", VETH1],
        capture_output=True,
        check=False,
    )
    subprocess.run(["ip", "link", "set", VETH0, "up"], check=True)
    subprocess.run(["ip", "link", "set", VETH1, "up"], check=True)
    yield
    subprocess.run(["ip", "link", "del", VETH0], capture_output=True, check=False)


@pytest.mark.integration
def test_scan_finds_el7201_identity(veth_up):
    so = _lib_path()
    if not os.path.isfile(so):
        pytest.skip("build libul_ecat.so missing")

    sim_py = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "scripts", "ecat_slave_sim.py"))
    sim = subprocess.Popen(
        [sys.executable, sim_py, VETH1],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(0.3)
    try:
        lib = ctypes.CDLL(so)

        from ecat_ctypes import UlEcatMasterSettings, UlEcatMasterSlavesT, bind_master_api
        bind_master_api(lib)

        mac = list(_read_mac(VETH0))
        cfg = UlEcatMasterSettings()
        cfg.iface_name = VETH0.encode("utf-8")
        cfg.dst_mac = (ctypes.c_uint8 * 6)(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)
        cfg.src_mac = (ctypes.c_uint8 * 6)(*mac)
        cfg.rt_priority = 0
        cfg.dc_enable = 0
        cfg.dc_sync0_cycle = 1_000_000

        assert lib.ul_ecat_master_init(ctypes.byref(cfg)) == 0
        try:
            assert lib.ul_ecat_scan_network() == 0
        finally:
            lib.ul_ecat_master_shutdown()

        dbp = lib.ul_ecat_get_master_slaves()
        assert dbp
        db = dbp.contents
        assert db.slave_count >= 1
        assert db.slaves[0].vendor_id == EXP_VENDOR
        assert db.slaves[0].product_code == EXP_PRODUCT
    finally:
        sim.terminate()
        try:
            sim.wait(timeout=2)
        except subprocess.TimeoutExpired:
            sim.kill()