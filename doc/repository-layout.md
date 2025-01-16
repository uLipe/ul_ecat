# Repository layout

```
ul_ecat/
  CMakeLists.txt          # Project, library, tests, optional ul_ecat_coverage target
  cmake/
    ul_ecatConfig.cmake.in
  include/
    ul_ecat_master.h      # Public C API (master lifecycle, queue, scan, CLI)
    ul_ecat_frame.h       # Frame/datagram encode/decode (tests + internal)
    ul_ecat_al.h          # AL Control / Status bit helpers
  src/
    ul_ecat_al.c
    ul_ecat_frame.c       # Encode/decode
    ul_ecat_master.c      # Master implementation
  tools/
    ul_ecat_tool.c        # Optional thin main() → ul_ecat_app_execute
  scripts/
    ul_ecat_tool.py       # Python CLI (ctypes)
    ecat_slave_sim.py     # EL7201-like L2 responder for veth experiments
  tests/
    CMakeLists.txt
    test_al.cpp           # GoogleTest (AL helpers)
    test_frame.cpp
    test_frame_negative.cpp
    test_master_smoke.cpp
    test_tool.py          # pytest (shared lib + CLI smoke)
    test_integration_sim.py  # optional veth+sim (excluded from default ctest)
  doc/
    *.md
  README.md
  requirements.txt      # pytest (optional pytest-timeout for integration)
```

Build outputs (default `build/`):

- `libul_ecat.a`, `libul_ecat.so` (if shared enabled)
- `tests/ul_ecat_tests`
- `ecatTool` (if `UL_ECAT_BUILD_TOOLS=ON`)
- `coverage_html/` (if `cmake --build build --target ul_ecat_coverage` after configuring with coverage + lcov/genhtml)
