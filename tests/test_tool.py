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
    r = subprocess.run([sys.executable, script, "--help"], capture_output=True, text=True, timeout=5)
    assert r.returncode == 0
    assert "ul-ecat" in r.stdout or "-c" in r.stdout or "command" in r.stdout.lower()


def test_python_cli_batch_help_command():
    import subprocess
    import sys

    script = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "scripts", "ul_ecat_tool.py"))
    r = subprocess.run(
        [sys.executable, script, "-c", "help"],
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert r.returncode == 0


def test_slave_sim_imports():
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    path = os.path.join(root, "scripts", "ecat_slave_sim.py")
    assert os.path.isfile(path)
