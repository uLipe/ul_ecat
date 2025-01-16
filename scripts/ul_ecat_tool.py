#!/usr/bin/env python3
"""
CLI for ul_ecat: loads libul_ecat.so and calls ul_ecat_app_execute (same argv as the C tool).
Requires CAP_NET_RAW for raw sockets used by the library.
"""
from __future__ import annotations

import ctypes
import os
import sys


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


def main() -> int:
    libpath = _find_library()
    try:
        lib = ctypes.CDLL(libpath)
    except OSError as exc:
        print(f"Failed to load {libpath}: {exc}", file=sys.stderr)
        print("Build with -DUL_ECAT_BUILD_SHARED=ON and set UL_ECAT_LIB if needed.", file=sys.stderr)
        return 1

    lib.ul_ecat_app_execute.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
    lib.ul_ecat_app_execute.restype = ctypes.c_int

    argc = len(sys.argv)
    bufs = [ctypes.create_string_buffer(s.encode("utf-8")) for s in sys.argv]
    argv = (ctypes.c_char_p * argc)()
    for i in range(argc):
        argv[i] = ctypes.cast(bufs[i], ctypes.c_char_p)

    return int(lib.ul_ecat_app_execute(argc, argv))


if __name__ == "__main__":
    raise SystemExit(main())
