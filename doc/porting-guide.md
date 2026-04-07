# Porting guide

For how layers and platform glue fit together, see [`architecture.md`](architecture.md).

## Linux (primary)

- Use `CAP_NET_RAW` or root for raw sockets.
- For real-time scheduling, grant `rtprio` in `/etc/security/limits.conf` or run with `CAP_SYS_NICE`. If `SCHED_FIFO` or `mlockall` fails, this stack emits a **warning** and continues without hard RT guarantees.
- **PREEMPT_RT** is optional; without it you still get best-effort POSIX scheduling.

## Other operating systems

The master core uses [`ul_ecat_osal.h`](../include/ul_ecat_osal.h) and [`ul_ecat_transport.h`](../include/ul_ecat_transport.h). Implementations in this repo:

- **Linux:** `src/osal/osal_linux.c`, `src/transport/transport_linux.c`
- **Zephyr:** [`doc/zephyr-module.md`](zephyr-module.md)
- **NuttX:** [`doc/nuttx-module.md`](nuttx-module.md)

For a new OS, add `osal_*.c` + `transport_*.c` and wire them in your build system. For BSD/Windows, replace the transport with BPF / Npcap (not included); keep `ul_ecat_frame.c` unchanged if the wire format stays identical.

## Embedded CMake consumption

- Prefer the **static** archive `libul_ecat.a` and link `pthread`.
- Disable tests and Python on the toolchain: `-DUL_ECAT_BUILD_TESTS=OFF -DUL_ECAT_BUILD_SHARED=OFF`.
- Provide your own `main` and call the public API instead of `ul_ecat_app_execute`.

## Unity / bare-metal

GoogleTest targets the host. For MCU builds, compile `ul_ecat_frame.c` under Unity or similar and stub the transport layer.
