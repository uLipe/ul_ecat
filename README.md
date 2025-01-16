# ul_ecat

[![CI](https://github.com/uLipe/ul_ecat/actions/workflows/ci.yml/badge.svg)](https://github.com/uLipe/ul_ecat/actions/workflows/ci.yml)

Minimal **EtherCAT master** library for **Linux** using **AF_PACKET** raw sockets only (no kernel IgH stack). The wire format follows the public EtherCAT frame/datagram description (length field, 10-byte datagram header, data, working counter). This is an educational / bring-up stack, not a certified commercial EtherCAT master.

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
- **Python** `scripts/ul_ecat_tool.py` (ctypes) and optional `scripts/ecat_slave_sim.py` for L2 experiments

SoE/FoE are **out of scope** for this MVP.

## CI

GitHub Actions runs **configure → build → ctest** on pushes and pull requests to `master` / `main` (see [`.github/workflows/ci.yml`](.github/workflows/ci.yml)).

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

- `build/libul_ecat.a` — static library (default for embedded)
- `build/libul_ecat.so` — shared library (when `UL_ECAT_BUILD_SHARED=ON`, for Python ctypes)

### Options

| Option | Default | Meaning |
|--------|---------|---------|
| `UL_ECAT_BUILD_TESTS` | ON | GoogleTest + pytest (if `pytest` found) |
| `UL_ECAT_BUILD_SHARED` | ON | Build `libul_ecat.so` |
| `UL_ECAT_BUILD_TOOLS` | OFF | Legacy C `ecatTool` executable |
| `UL_ECAT_ENABLE_COVERAGE` | OFF | Add `--coverage` (gcov/lcov) |

## Install

```bash
cmake --install build --prefix /usr/local
```

Consuming from CMake:

```cmake
find_package(ul_ecat REQUIRED)
target_link_libraries(my_app PRIVATE ul_ecat::ul_ecat)
```

## Tests

```bash
cd build && ctest --output-on-failure
```

`ctest` runs **GoogleTest** and **pytest** on `tests/`, but **skips** [`tests/test_integration_sim.py`](tests/test_integration_sim.py) (veth + raw socket + root; can block or hang if run unintentionally).

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

Install `lcov` and `genhtml`, then configure with `UL_ECAT_ENABLE_COVERAGE=ON` so the library and tests are built with `--coverage`.

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

```bash
export UL_ECAT_LIB=$PWD/build/libul_ecat.so   # if not found automatically
python3 scripts/ul_ecat_tool.py eth0 scan
python3 scripts/ul_ecat_tool.py eth0 read 0x1000 0x0130 2
```

Same arguments as the C `ul_ecat_app_execute` / legacy `ecatTool`.

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

- `doc/architecture.md` — layers and data flow
- `doc/mental-model.md` — ADP/ADO, WKC, AL states
- `doc/repository-layout.md` — repository map
- `doc/porting-guide.md` — non-Linux / embedded notes

## License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE). Copyright (c) 2026 Felipe Neves.

## Limitations

- Linux-only transport (other OSes need a different raw socket layer).
- No full PDO configuration, CoE mailbox, or certified conformance.
- DC path is minimal and assumes a single slave context for demo reads.
