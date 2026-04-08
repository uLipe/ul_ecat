# Repository layout

```
ul_ecat/
  CMakeLists.txt          # Project, ul_ecat_wire, master/slave libs, tests, optional coverage target
  cmake/
    ul_ecatConfig.cmake.in
  generated/                # gen_slave_data.py output (identity tables); optional regen target
    ul_ecat_slave_tables.c
    ul_ecat_slave_tables.h
  include/
    ul_ecat_master.h        # Public C API (master lifecycle, queue, scan, CLI)
    ul_ecat_slave.h         # Public C API (slave init + Ethernet/PDU processing)
    ul_ecat_esc_regs.h      # ESC register offsets (shared)
    ul_ecat_frame.h         # Frame/datagram encode/decode (tests + internal)
    ul_ecat_al.h            # AL Control / Status bit helpers
    ul_ecat_osal.h          # OS abstraction — Linux / Zephyr / NuttX impl.
    ul_ecat_transport.h     # Raw L2 transport — per-OS implementation
  src/
    common/
      ul_ecat_frame.c       # Encode/decode (linked via ul_ecat_wire)
      ul_ecat_al.c
    master/
      ul_ecat_master.c      # Master core (calls OSAL + transport only)
    slave/
      ul_ecat_esc.c
      ul_ecat_slave_pdu.c
      ul_ecat_slave.c
    osal/
      osal_linux.c
      osal_zephyr.c
      osal_nuttx.c
    transport/
      transport_linux.c
      transport_zephyr.c
      transport_zephyr_netdev.c
      transport_nuttx.c
  zephyr/
    module.yml
    Kconfig
    CMakeLists.txt
  nuttx/
    Kconfig
    Make.defs
    Make.defs.slave_lan9252
    ul_ecat_sources.cmake
    ul_ecat_slave_lan9252_sources.cmake
    CMakeLists.txt
  samples/
    common/
      ul_ecat_servo_sample.h
    zephyr/ul_ecat_scan/
    zephyr/ul_ecat_servo/
    nuttx/ul_ecat_scan/
    nuttx/ul_ecat_servo/
  tools/
    ul_ecat_tool.c          # Optional thin main() → ul_ecat_app_execute
    ul_ecat_slave_harness.c # TCP loopback server for slave tests
  scripts/
    ci_build_zephyr_samples.sh   # CI: west + SDK + build scan + servo samples
    ci_verify_nuttx_integration.sh
    ul_ecat_tool.py         # Python CLI (ctypes master + slave-emulator)
    gen_slave_data.py       # Generates generated/ul_ecat_slave_tables.{c,h}
    ethercat_controller_sim.py  # Stateful scan client for harness tests
    ecat_slave_sim.py       # EL7201-like L2 responder for veth experiments
  tests/
    CMakeLists.txt
    test_*.cpp              # GoogleTest
    test_tool.py            # pytest (shared lib + CLI smoke)
    test_socket_harness.py  # pytest (harness + controller simulator)
    test_integration_sim.py # optional veth+sim (excluded from default ctest)
  controllers/
    lan9252/              # Optional LAN9252 SPI bridge (UL_ECAT_BUILD_LAN9252)
      ports/              # RTOS HALs (Zephyr, NuttX) — see ports/README.md
        zephyr/hal_zephyr.c
        nuttx/hal_nuttx.c
        nuttx/hal_nuttx.h
  doc/
    *.md
    lan9252-datasheet-reference.md
  README.md
  requirements.txt
```

**Embedded quick start:** [README.md § Quick start](../README.md#quick-start-zephyr-and-nuttx), then [zephyr-module.md](zephyr-module.md) or [nuttx-module.md](nuttx-module.md).

Build outputs (default `build/`):

- `libul_ecat_wire.a` — shared frame + AL objects
- `libul_ecat.a`, `libul_ecat.so` (if shared enabled) — **master**
- `libul_ecat_slave.a` — **slave** (if enabled)
- `ul_ecat_slave_harness` — TCP test server (if slave enabled)
- `tests/ul_ecat_tests`, `tests/ul_ecat_slave_tests`
- `ecatTool` (if `UL_ECAT_BUILD_TOOLS=ON`)
- `coverage_html/` (if `cmake --build build --target ul_ecat_coverage` after configuring with coverage + lcov/genhtml)
