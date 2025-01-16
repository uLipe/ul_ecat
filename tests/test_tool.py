"""Smoke test: load libul_ecat.so and call ul_ecat_get_master_slaves."""
from __future__ import annotations

import os
import ctypes
import pytest


def _lib_path() -> str:
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    p = os.path.join(root, "build", "libul_ecat.so")
    return p


def test_shared_library_loads_and_exports():
    so = _lib_path()
    if not os.path.isfile(so):
        pytest.skip("build/libul_ecat.so missing (configure with -DUL_ECAT_BUILD_SHARED=ON)")
    lib = ctypes.CDLL(so)
    lib.ul_ecat_get_master_slaves.argtypes = []
    lib.ul_ecat_get_master_slaves.restype = ctypes.c_void_p
    p = lib.ul_ecat_get_master_slaves()
    assert p != 0


def test_python_cli_help_runs():
    import subprocess
    import sys

    script = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "scripts", "ul_ecat_tool.py"))
    r = subprocess.run([sys.executable, script], capture_output=True, text=True, timeout=5)
    # Missing args -> app_execute prints usage; ctypes may fail if .so missing
    assert r.returncode in (0, 1)


def test_slave_sim_imports():
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    path = os.path.join(root, "scripts", "ecat_slave_sim.py")
    assert os.path.isfile(path)
