# Host simulator: TCP harness and EtherCAT controller simulator

This project supports **automated tests** that exercise the **C slave library** without **AF_PACKET** or root privileges. Two components work together:

1. **`ul_ecat_slave_harness`** — a small C executable that links `libul_ecat_slave` and speaks a simple TCP protocol on the loopback interface.
2. **`scripts/ethercat_controller_sim.py`** — a **stateful EtherCAT controller simulator** (not a canned mock): it builds real frames using the same field layout as `src/common/ul_ecat_frame.c` and runs a **discovery-style** sequence (APWR configured station address, then FPRD identity registers).

Timing is **not** real-time: delays are only those needed for process startup and socket I/O.

## TCP framing

Both directions use the same format:

1. **Four bytes**, big-endian unsigned length **N**.
2. **N bytes** of a full **Ethernet frame** starting with destination MAC (same layout as on the wire; EtherType `0x88A4`).

If processing fails, the harness may send **N = 0** as an error indicator (length-only).

## Running the harness

After building with `-DUL_ECAT_BUILD_SLAVE=ON`:

```text
./build/ul_ecat_slave_harness -p 9234
```

The harness listens on **127.0.0.1** and accepts a connection. It initializes the slave with identity from **`generated/ul_ecat_slave_tables.c`** (regenerated via `scripts/gen_slave_data.py` or CMake target `ul_ecat_regen_slave_tables`).

## Running the controller simulator

In another terminal:

```text
python3 scripts/ethercat_controller_sim.py -p 9234
```

It prints decoded **vendor**, **product**, **revision**, and **serial** read from the slave.

## CLI wrapper

```text
python3 scripts/ul_ecat_tool.py slave-emulator -p 9234
```

Starts the harness subprocess, runs the scan, prints the result, then stops the harness. Optional **`UL_ECAT_SLAVE_HARNESS`** selects the harness binary path.

## Tests

`tests/test_socket_harness.py` spawns the harness and calls **`run_identity_scan`** from the simulator module; it is part of the default **`pytest`** run under `ctest` when `pytest` is installed.

## Full E2E: C master + L2 slave simulator (not TCP)

To run the **real** `libul_ecat` master against the Python **L2** responder (`ecat_slave_sim.py`), use a **veth** pair and root — same idea as [`tests/test_integration_sim.py`](../tests/test_integration_sim.py), with verbose logs:

```bash
sudo env UL_ECAT_VERBOSE=1 UL_ECAT_LIB=$PWD/build/libul_ecat.so \
  python3 scripts/run_e2e_l2_scan.py
```

You should see interleaved lines from **`[ul_ecat] scan`** (master) and **`[ecat_slave_sim]`** (slave). This exercises **scan** (APWR + identity FPRDs), not the TCP harness.

**CI:** the same script runs in **GitHub Actions** after `ctest` (see `.github/workflows/ci.yml`), with `sudo` on the Ubuntu runner.

## Related documents

- [`architecture.md`](architecture.md) — project-wide view
- [`slave-architecture.md`](slave-architecture.md) — slave layers
