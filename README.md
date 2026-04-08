# ul_ecat

[![CI](https://github.com/uLipe/ul_ecat/actions/workflows/ci.yml/badge.svg)](https://github.com/uLipe/ul_ecat/actions/workflows/ci.yml)

Minimal **EtherCAT master** and optional **slave** libraries for **Linux** host builds. The master uses **AF_PACKET** raw sockets only (no kernel IgH stack). The wire format follows the public EtherCAT frame/datagram description (length field, 10-byte datagram header, data, working counter). This is an educational / bring-up stack, not a certified commercial EtherCAT stack.

EtherCAT® is a registered trademark. This project is not affiliated with EtherCAT Technology Group.

## Features

- ETG-style **EtherCAT PDU** (length + reserved) and **datagrams** with **WKC**
- **FPRD / FPWR / APWR** for register access and station addressing
- **Ring scan**: auto-increment **APWR** to `0x0010` (configured station address), then identity reads
- **AL Control / AL Status** (`0x0120` / `0x0130`) with real polling (no fake success)
- Optional **Distributed Clocks** hooks (system time read at `0x0910`)
- **POSIX** periodic thread with **SCHED_FIFO** / `mlockall` — on failure, **warnings** and fallback (non-fatal)
- **CMake** install + `ul_ecatConfig.cmake` for embedding in other projects
- **GoogleTest** unit tests and **pytest** smoke tests for the Python CLI
- **`libul_ecat_slave`** — ESC register mirror + PDU replies (**FPRD / FPWR / APWR**); shared **`ul_ecat_wire`** (`src/common/`) with the master; optional **`ul_ecat_slave_controller`** ([`include/ul_ecat_slave_controller.h`](include/ul_ecat_slave_controller.h)) for app-facing **poll + callbacks** (software Ethernet vs LAN9252 SPI when `UL_ECAT_BUILD_LAN9252=ON`)
- **Host test link**: `tools/ul_ecat_slave_harness` (TCP) + `scripts/ethercat_controller_sim.py` (stateful scan, not a mock)
- **Python** `scripts/ul_ecat_tool.py` (interactive/batch master CLI, legacy `ul_ecat_app_execute`, + `slave-emulator`) and optional `scripts/ecat_slave_sim.py` for raw L2 experiments
- **Zephyr** module (Kconfig + CMake): [`doc/zephyr-module.md`](doc/zephyr-module.md) — sample [`samples/zephyr/ul_ecat_scan`](samples/zephyr/ul_ecat_scan) runs `ul_ecat_scan_network` with `ZEPHYR_EXTRA_MODULES` pointing at this repo
- **NuttX** (Kconfig + Make/CMake helpers): [`doc/nuttx-module.md`](doc/nuttx-module.md) — sample [`samples/nuttx/ul_ecat_scan`](samples/nuttx/ul_ecat_scan), sources under [`nuttx/`](nuttx/)

SoE/FoE are **out of scope** for this MVP.

## Quick start: Zephyr and NuttX

Use this repository as an **out-of-tree** module: the portable master ([`src/master/ul_ecat_master.c`](src/master/ul_ecat_master.c)) links against an **OSAL** and **transport** implementation for each RTOS. Full details: [`doc/zephyr-module.md`](doc/zephyr-module.md), [`doc/nuttx-module.md`](doc/nuttx-module.md).

### Zephyr

**Prerequisites:** Zephyr SDK, `west`, a board defconfig with Ethernet and packet sockets (see the sample `prj.conf`).

1. Point `ZEPHYR_EXTRA_MODULES` at the **clone root** of this repo (the folder that contains [`zephyr/module.yml`](zephyr/module.yml)).
2. Build the scan sample (example: `native_sim` with host Ethernet):

   ```bash
   export ZEPHYR_EXTRA_MODULES=/path/to/ul_ecat
   west build -b native_sim/native/64 -p always /path/to/ul_ecat/samples/zephyr/ul_ecat_scan
   west build -t run
   ```

3. In [`samples/zephyr/ul_ecat_scan/src/main.c`](samples/zephyr/ul_ecat_scan/src/main.c), set the interface name (`UL_ECAT_SAMPLE_IFACE_NAME`, default `eth0`) to match your `net_if`. Enable `CONFIG_UL_ECAT=y` and `CONFIG_UL_ECAT_MASTER=y` (see sample `prj.conf`).

Optional: `CONFIG_UL_ECAT_TRANSPORT_NETDEV=y` for TX via `net_if` instead of `zsock_send` (see Zephyr doc).

### NuttX

**Prerequisites:** NuttX + `nuttx-apps`, a board with `CONFIG_NET`, `CONFIG_NET_PKT`, and an Ethernet driver.

1. **Kconfig:** In `apps/Kconfig`, source [`nuttx/Kconfig`](nuttx/Kconfig) and [`samples/nuttx/ul_ecat_scan/Kconfig`](samples/nuttx/ul_ecat_scan/Kconfig) (absolute paths to your clone).
2. **App:** Copy or symlink [`samples/nuttx/ul_ecat_scan`](samples/nuttx/ul_ecat_scan) under `apps/examples/` (or keep it in-tree and set paths). Enable `CONFIG_UL_ECAT`, `CONFIG_UL_ECAT_MASTER`, and `CONFIG_EXAMPLES_UL_ECAT_SCAN` in `menuconfig`. Merge options from [`samples/nuttx/ul_ecat_scan/defconfig.snippet`](samples/nuttx/ul_ecat_scan/defconfig.snippet) as needed.
3. **Build:** From the NuttX tree, `./tools/configure.sh <board>:<config>` then `make -j`. Set **`UL_ECAT_ROOT`** in the app `Makefile` if the sample is not under the `ul_ecat` repo (see [`doc/nuttx-module.md`](doc/nuttx-module.md)).
4. **Run:** `nsh> ul_ecat_scan [eth0]` — optional argument is the interface name.

## CI

GitHub Actions runs **configure → build → ctest** on pushes and pull requests to `master` / `main` (see [`.github/workflows/ci.yml`](.github/workflows/ci.yml)), then an **E2E** step: **`scripts/run_e2e_l2_scan.py`** with `sudo` (veth + raw sockets + verbose master scan).

## Requirements

- Linux with `AF_PACKET` and `ETH_P_ETHERCAT` (`0x88A4`)
- CMake ≥ 3.16, C99, C++17 (tests only)
- Python 3.10+ for pytest / scripts (optional)
- Raw sockets usually require **root** or `CAP_NET_RAW`

## Build

```bash
cmake -S . -B build \
  -DUL_ECAT_BUILD_TESTS=ON \
  -DUL_ECAT_BUILD_SHARED=ON \
  -DUL_ECAT_BUILD_TOOLS=OFF
cmake --build build -j
```

Artifacts:

- `build/libul_ecat.a` — static **master** library (default for embedded)
- `build/libul_ecat.so` — shared master library (when `UL_ECAT_BUILD_SHARED=ON`, for Python ctypes)
- `build/libul_ecat_slave.a` — static **slave** library (when `UL_ECAT_BUILD_SLAVE=ON`)
- `build/libul_ecat_wire.a` — shared **frame + AL** helpers (linked by master and slave)
- `build/ul_ecat_slave_harness` — TCP loopback server for tests (when slave is built)

### Options

| Option | Default | Meaning |
|--------|---------|---------|
| `UL_ECAT_BUILD_MASTER` | ON | Build `libul_ecat` (master) |
| `UL_ECAT_BUILD_SLAVE` | ON | Build `libul_ecat_slave` + `ul_ecat_slave_harness` |
| `UL_ECAT_BUILD_TESTS` | ON | GoogleTest + pytest (if `pytest` found) |
| `UL_ECAT_BUILD_SHARED` | ON | Build `libul_ecat.so` |
| `UL_ECAT_BUILD_TOOLS` | OFF | Legacy C `ecatTool` executable |
| `UL_ECAT_ENABLE_COVERAGE` | OFF | Add `--coverage` (gcov/lcov) for all compiled targets in the build |
| `UL_ECAT_BUILD_LAN9252` | OFF | Build `libul_ecat_lan9252` ([`controllers/lan9252`](controllers/lan9252/README.md)) — SPI bridge only; can be built without master/slave |

## Install

```bash
cmake --install build --prefix /usr/local
```

Consuming from CMake:

```cmake
find_package(ul_ecat REQUIRED)
target_link_libraries(my_app PRIVATE ul_ecat::ul_ecat)
# Optional slave:
# target_link_libraries(my_slave PRIVATE ul_ecat::slave)
```

## Tests

```bash
cd build && ctest --output-on-failure
```

`ctest` runs **GoogleTest** and **pytest** on `tests/`, but **skips** [`tests/test_integration_sim.py`](tests/test_integration_sim.py) (duplicate of the L2 E2E scenario; kept for manual `pytest`).

The **L2 master+slave E2E** with logs is **`scripts/run_e2e_l2_scan.py`** — run locally with `sudo`, and in **CI** as a dedicated workflow step after `ctest`.

**Integration test** (manual, as root):

```bash
sudo env UL_ECAT_INTEGRATION=1 python3 -m pytest tests/test_integration_sim.py -v --timeout=60
```

(`pip install pytest-timeout` for `--timeout`, optional.)

Or:

```bash
./build/tests/ul_ecat_tests
python3 -m pytest tests/ -v --ignore=tests/test_integration_sim.py
```

Coverage (gcov + lcov):

Install `lcov` and `genhtml`, then configure with `UL_ECAT_ENABLE_COVERAGE=ON` so libraries, tests, and the slave harness are built with `--coverage` where applicable.

```bash
cmake -S . -B build -DUL_ECAT_ENABLE_COVERAGE=ON -DUL_ECAT_BUILD_TESTS=ON
cmake --build build
cd build && ctest
```

Or use the optional CMake target (only generated if `lcov` and `genhtml` are found at configure time):

```bash
cmake -S . -B build -DUL_ECAT_ENABLE_COVERAGE=ON -DUL_ECAT_BUILD_TESTS=ON
cmake --build build --target ul_ecat_coverage
# Report: build/coverage_html/index.html
```

Manual `lcov`/`genhtml` from `build/` works the same as documented in the coverage target.

## Python CLI

**Master** (ctypes against `libul_ecat.so`):

- **Privileges:** raw Ethernet (`AF_PACKET`) needs **CAP_NET_RAW** — run with **`sudo`** (or set `cap_net_raw` on the Python binary; see tool help text). This is normal Linux behaviour, not a missing dependency in the repo.
- **Interactive** (TTY, no arguments): REPL similar to `bluetoothctl` — `interface <iface>`, `scan`, `list`, `read`, `write`, `help`, `quit`.
- **Batch**: `python3 scripts/ul_ecat_tool.py -c 'interface eth0; scan; list'` (semicolon-separated commands).
- **Legacy** (same as C `ul_ecat_app_execute` / `ecatTool`): `python3 scripts/ul_ecat_tool.py <iface> <command> [args...]`.

```bash
export UL_ECAT_LIB=$PWD/build/libul_ecat.so   # if not found automatically
python3 scripts/ul_ecat_tool.py eth0 scan
python3 scripts/ul_ecat_tool.py eth0 read 0x1000 0x0130 2
python3 scripts/ul_ecat_tool.py -c "help"
```

**Slave emulator** (loopback TCP harness + controller simulator; no raw sockets):

```bash
python3 scripts/ul_ecat_tool.py slave-emulator -p 9234
```

Optional: `UL_ECAT_SLAVE_HARNESS=/path/to/ul_ecat_slave_harness`. See [`doc/simulator.md`](doc/simulator.md).

## E2E: real master + simulated slave on L2 (veth)

The TCP harness (`ul_ecat_slave_harness`) only tests **`libul_ecat_slave`** with a Python controller. To run the **C master** (`libul_ecat.so`) against a **simulated slave** on the same machine, use a **veth** pair and `scripts/ecat_slave_sim.py` (requires **root**).

Verbose scan trace from the master:

```bash
sudo env UL_ECAT_VERBOSE=1 UL_ECAT_LIB=$PWD/build/libul_ecat.so \
  python3 scripts/run_e2e_l2_scan.py
```

This prints **`[ul_ecat] scan`** lines (APWR/FPRD) and **`[ecat_slave_sim]`** datagram logs. See [`doc/simulator.md`](doc/simulator.md).

## Slave identity tables (generator)

```bash
python3 scripts/gen_slave_data.py --vendor 0x2 --product 0x1C213052
cmake --build build --target ul_ecat_regen_slave_tables   # if CMake found Python 3
```

Writes `generated/ul_ecat_slave_tables.{c,h}` (committed for reproducible CI).

## Slave simulator (optional)

`scripts/ecat_slave_sim.py` emulates a **single** EtherCAT slave with **Beckhoff EL7201-like** identity (Vendor `0x00000002`, Product `0x1C213052` by default; override with `UL_ECAT_SIM_VENDOR_ID`, `UL_ECAT_SIM_PRODUCT_CODE`, etc.).

For isolated L2 tests, create a **veth** pair and run the simulator on one peer and the master on the other (requires root):

```bash
sudo ip link add veth0 type veth peer name veth1
sudo ip link set veth0 up
sudo ip link set veth1 up
sudo python3 scripts/ecat_slave_sim.py veth1
```

## Documentation

See the `doc/` directory:

- **Embedded quick reference:** [`doc/zephyr-module.md`](doc/zephyr-module.md), [`doc/nuttx-module.md`](doc/nuttx-module.md) (and the [Quick start](#quick-start-zephyr-and-nuttx) above)
- `doc/architecture.md` — **layers, OSAL/transport separation, diagrams, threading** (start here for structure)
- `doc/mental-model.md` — ADP/ADO, WKC, AL states (master-centric)
- `doc/slave-architecture.md`, `doc/slave-mental-model.md` — **slave** stack and **`ul_ecat_slave_controller`**
- **Master** event hooks: `ul_ecat_register_dc_callback`, `ul_ecat_register_frame_callback`, `ul_ecat_eventloop_run` ([`include/ul_ecat_master.h`](include/ul_ecat_master.h)) — no duplicate “generic” callback API added for scan completion; use the existing DB after `ul_ecat_scan_network`
- `doc/simulator.md` — TCP harness + **EtherCAT controller simulator** (`scripts/ethercat_controller_sim.py`)
- `doc/repository-layout.md` — repository map
- `doc/porting-guide.md` — OSAL/transport ports (Linux, Zephyr, NuttX, others)

## License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE). Copyright (c) 2026 Felipe Neves.

## Limitations

- **Linux** builds use AF_PACKET only; **Zephyr** and **NuttX** use their own OSAL/transport (see `doc/`). Other OSes still need a custom transport.
- No full PDO configuration, CoE mailbox, or certified conformance.
- DC path is minimal and assumes a single slave context for demo reads.
