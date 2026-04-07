# Slave stack architecture

This document describes the **minimal EtherCAT slave** implementation in `ul_ecat`: ESC register mirror, PDU handling, and how it depends on the shared wire-format layer.

## Goals (phase 1)

- Parse incoming EtherCAT **PDU datagrams** using the same layout as the master (`ul_ecat_frame.h`).
- Maintain a **byte-addressable ESC RAM** mirror (4 KiB) with identity registers, configured station address, and AL Control / Status.
- Build **reply Ethernet frames** (EtherType `0x88A4`) with updated datagram data and **WKC** per processed datagram.
- No hardware driver, no full EEPROM/SII in this phase unless a future test requires it.

## Layering

```mermaid
flowchart TB
  subgraph wire [Shared wire — ul_ecat_wire]
    F[ul_ecat_frame.c]
    A[ul_ecat_al.c]
  end
  subgraph slave [libul_ecat_slave]
    API[ul_ecat_slave.c — Ethernet wrap]
    PDU[ul_ecat_slave_pdu.c — datagram walk]
    ESC[ul_ecat_esc.c — register file]
    TBL[generated ul_ecat_slave_tables.c]
  end
  API --> PDU
  API --> F
  PDU --> F
  PDU --> ESC
  TBL -.-> API
```

- **`ul_ecat_slave_process_ethernet`** validates the Ethernet header and EtherType, extracts the ECAT payload, calls **`ul_ecat_slave_process_pdu`**, then **`ul_ecat_build_eth_frame`** with the slave’s MAC as source and the received frame’s source as destination (reply to master).
- **`ul_ecat_slave_process_pdu`** iterates datagrams with `ul_ecat_pdu_count_datagrams` / `ul_ecat_dgram_parse`, dispatches **FPRD / FPWR / APWR**, and re-encodes each datagram with **`ul_ecat_dgram_encode`** and the new WKC/data.

## Identity and tables

Default identity for the harness and generated tables is produced by **`scripts/gen_slave_data.py`**, which writes `generated/ul_ecat_slave_tables.{c,h}` and prints a review table. CMake target **`ul_ecat_regen_slave_tables`** runs the generator on demand.

## Related documents

- [`slave-mental-model.md`](slave-mental-model.md) — addressing and WKC from the slave perspective
- [`architecture.md`](architecture.md) — overall project layers
- [`simulator.md`](simulator.md) — TCP harness and Python controller simulator
