# Slave mental model (ADP, ADO, WKC)

This document complements [`mental-model.md`](mental-model.md) with the **slave** view: how incoming datagrams select registers and how the working counter is updated.

## Addressing

- **ADO** (address in ESC): byte offset into the ESC register space (e.g. `0x0012` vendor ID). The slave reads or writes its **RAM mirror** at that offset.
- **ADP** meaning depends on **command**:
  - **FPRD / FPWR:** **configured station address** — must match the value stored at **`0x0010`** after the master has programmed it (typically via **APWR** on the auto-increment ring).
  - **APWR:** **auto-increment position** — compared to `ul_ecat_slave_t.logical_position` (usually `0` for the first slave on the ring).

Until the station address is set and matches, **FPRD**/**FPWR** to that station do not succeed (WKC stays `0`).

## Working counter (WKC)

For each datagram, if the slave applies the operation successfully (read/write of the requested length), it sets **WKC = 1** in the reply. Otherwise **WKC = 0**. This matches the minimal behavior used in unit tests and in the host harness.

## AL registers

**AL Control** (`0x0120`) and **AL Status** (`0x0130`) live in the same RAM mirror. Phase-1 behavior focuses on a consistent **AL Status** in **INIT** for discovery; full AL state machine transitions can be extended later.

## Related documents

- [`slave-architecture.md`](slave-architecture.md) — code layout
- [`mental-model.md`](mental-model.md) — master-centric PDU and field layout
