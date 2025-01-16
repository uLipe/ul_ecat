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

# Beckhoff EL7201-like defaults (must match scripts/ecat_slave_sim.py)
EXP_VENDOR = 0x00000002
EXP_PRODUCT = 0x1C213052

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

        mac = list(_read_mac(VETH0))
        cfg = MasterSettings()
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

        lib.ul_ecat_get_master_slaves.restype = ctypes.c_void_p
        dbp = lib.ul_ecat_get_master_slaves()
        assert dbp

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
        assert db.slave_count >= 1
        assert db.slaves[0].vendor_id == EXP_VENDOR
        assert db.slaves[0].product_code == EXP_PRODUCT
    finally:
        sim.terminate()
        try:
            sim.wait(timeout=2)
        except subprocess.TimeoutExpired:
            sim.kill()