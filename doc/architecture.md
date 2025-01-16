# Architecture

## Layers

1. **Wire format** (`ul_ecat_frame.c` / `ul_ecat_frame.h`): builds and parses Ethernet + EtherCAT PDU + datagrams (little-endian fields, WKC after data).
2. **AL helpers** (`ul_ecat_al.c` / `ul_ecat_al.h`): AL Control / AL Status bit masks, acknowledge bit, and error indication (used by the master and unit tests).
3. **Master logic** (`ul_ecat_master.c`): slave database, scan (APWR + identity reads), AL state machine via blocking FPRD/FPWR, optional DC queue, event loop.
4. **Transport**: Linux `AF_PACKET` `SOCK_RAW` bound to `ETH_P_ETHERCAT` (`0x88A4`).

## Threads and locking

- A **periodic thread** runs `dc_cycle`, `ul_ecat_send_batched_frames`, and `ul_ecat_receive_frames_nonblock` on a 1 ms tick (configurable constant in code).
- A **mutex** (`g_trx_mutex`) serializes raw socket access between the periodic thread and **blocking** exchanges used during scan / AL polling (so the CLI does not race the cyclic thread).

## Queue

Datagrams are stored in **fixed buffers** (no per-datagram `malloc` in the hot path). The queue is protected by `g_q_mutex`.

## Event loop

DC and frame events are posted to a bounded queue consumed by `ul_ecat_eventloop_run()` in the application thread (callbacks must not block the RT thread if you extend this design).
