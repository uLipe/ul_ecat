# SPDX-License-Identifier: MIT
"""
Shared ctypes struct layouts that mirror the C public API (ul_ecat_master.h).

Import from here instead of defining MasterSettings / Slave / SlavesDb in every script.
"""
from __future__ import annotations

import ctypes

UL_ECAT_MAX_SLAVES = 16


class UlEcatMasterSettings(ctypes.Structure):
    _fields_ = [
        ("iface_name", ctypes.c_char_p),
        ("dst_mac", ctypes.c_ubyte * 6),
        ("src_mac", ctypes.c_ubyte * 6),
        ("rt_priority", ctypes.c_int),
        ("dc_enable", ctypes.c_int),
        ("dc_sync0_cycle", ctypes.c_uint32),
    ]


class UlEcatSlaveT(ctypes.Structure):
    _fields_ = [
        ("station_address", ctypes.c_uint16),
        ("state", ctypes.c_int),
        ("vendor_id", ctypes.c_uint32),
        ("product_code", ctypes.c_uint32),
        ("revision_no", ctypes.c_uint32),
        ("serial_no", ctypes.c_uint32),
        ("device_name", ctypes.c_char * 128),
    ]


class UlEcatMasterSlavesT(ctypes.Structure):
    _fields_ = [
        ("slaves", UlEcatSlaveT * UL_ECAT_MAX_SLAVES),
        ("slave_count", ctypes.c_int),
    ]


def bind_master_api(lib: ctypes.CDLL) -> None:
    """Set argtypes/restype for the commonly used master C API functions."""
    lib.ul_ecat_mac_broadcast.argtypes = [ctypes.POINTER(ctypes.c_ubyte)]
    lib.ul_ecat_mac_broadcast.restype = None

    lib.ul_ecat_master_init.argtypes = [ctypes.POINTER(UlEcatMasterSettings)]
    lib.ul_ecat_master_init.restype = ctypes.c_int

    lib.ul_ecat_master_shutdown.argtypes = []
    lib.ul_ecat_master_shutdown.restype = ctypes.c_int

    lib.ul_ecat_scan_network.argtypes = []
    lib.ul_ecat_scan_network.restype = ctypes.c_int

    lib.ul_ecat_get_master_slaves.argtypes = []
    lib.ul_ecat_get_master_slaves.restype = ctypes.POINTER(UlEcatMasterSlavesT)

    lib.ul_ecat_fprd_sync.argtypes = [
        ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_int,
    ]
    lib.ul_ecat_fprd_sync.restype = ctypes.c_int

    lib.ul_ecat_fpwr_sync.argtypes = [
        ctypes.c_uint16, ctypes.c_uint16, ctypes.c_void_p, ctypes.c_uint16, ctypes.c_int,
    ]
    lib.ul_ecat_fpwr_sync.restype = ctypes.c_int

    lib.ul_ecat_app_execute.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
    lib.ul_ecat_app_execute.restype = ctypes.c_int
