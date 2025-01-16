# Porting guide

## Linux (primary)

- Use `CAP_NET_RAW` or root for raw sockets.
- For real-time scheduling, grant `rtprio` in `/etc/security/limits.conf` or run with `CAP_SYS_NICE`. If `SCHED_FIFO` or `mlockall` fails, this stack emits a **warning** and continues without hard RT guarantees.
- **PREEMPT_RT** is optional; without it you still get best-effort POSIX scheduling.

## Other operating systems

This tree implements only **Linux** `linux/if_packet.h`. To port:

1. Replace `create_raw_socket()` and `bind()` with the OS-specific raw L2 API (e.g. BPF on BSD, WinPcap/Npcap on Windows — not included).
2. Keep `ul_ecat_frame.c` unchanged if the wire format stays identical.
3. Revisit mutex + threading model if your RTOS uses different primitives.

## Embedded CMake consumption

- Prefer the **static** archive `libul_ecat.a` and link `pthread`.
- Disable tests and Python on the toolchain: `-DUL_ECAT_BUILD_TESTS=OFF -DUL_ECAT_BUILD_SHARED=OFF`.
- Provide your own `main` and call the public API instead of `ul_ecat_app_execute`.

## Unity / bare-metal

GoogleTest targets the host. For MCU builds, compile `ul_ecat_frame.c` under Unity or similar and stub the transport layer.
