# EtherCAT specification conformance status

This document tracks which features from the EtherCAT specification (ETG.1000 and related) are implemented in ul_ecat, for both master and slave sides. It is updated as features are added.

Reference documents: ETG.1000 (protocol), ETG.2000 (ESI), ETG.2010 (SII), ETG.2200 (Slave Implementation Guide), ETG.7010 (Conformance Guide).

## Slave

| Feature | Status | Notes |
|---------|--------|-------|
| ESC register mirror (4 KiB) | Done | `src/slave/ul_ecat_esc.c` |
| Identity registers (vendor/product/rev/serial) | Done | Written at init from `ul_ecat_slave_identity_t` |
| PDU processing (FPRD, FPWR, APWR) | Done | `src/slave/ul_ecat_slave_pdu.c`; 256-byte datagram limit |
| AL State Machine (INIT/PREOP/SAFEOP/OP) | Done | `src/slave/ul_ecat_slave_al.c`; validates transitions, Error Indicator, AL Status Code (0x0134) |
| BOOT state | Rejected | Returns `AL_ERR_BOOTSTRAP_NOT_SUPPORTED` (0x0013) |
| AL Error acknowledge (ACK bit) | Done | Master ACK clears error indicator and status code |
| LAN9252 SPI controller backend | Done | `controllers/lan9252/`; CSR, ESC indirect, PDRAM |
| LAN9252 HAL (Zephyr, NuttX) | Done | `controllers/lan9252/ports/` |
| Software Ethernet backend | Done | For host simulation and tests |
| Controller abstraction + callbacks | Done | `ul_ecat_slave_controller.c`; AL Status and AL Event change callbacks |
| Process data (PDRAM, LAN9252) | Partial | Raw buffer I/O via `set_pdram`; no PDO mapping objects |
| SyncManager (SM0-SM3) | Basic | Register layout defined; SM0/SM1 mailbox config validated on INIT->PREOP transition; SM2/SM3 process data not yet validated |
| FMMU | Not yet | Not emulated in software backend; hardware-managed on LAN9252 |
| Mailbox protocol | Not yet | No SM0/SM1 mailbox handshake |
| CoE / Object Dictionary | Not yet | No SDO server, no OD entries |
| SII / EEPROM | Not yet | No SII content, no EEPROM read via ESC registers |
| ESI XML | Not yet | No generated XML device description |
| Distributed Clocks (slave side) | Not yet | No SYNC event handling |
| Emergency (EMCY) | Not yet | |
| Explicit Device Identification | Not yet | No alias address handling |
| Error counters / watchdog | Not yet | |
| Physical indicators (LED) | Not yet | |

## Master

| Feature | Status | Notes |
|---------|--------|-------|
| Transport (AF_PACKET Linux, Zephyr, NuttX) | Done | `src/transport/` |
| OSAL (Linux pthreads, Zephyr k_thread, NuttX pthreads) | Done | `src/osal/` |
| Network scan (APWR + FPRD identity) | Done | Linear scan until WKC=0; up to 16 slaves |
| AL state request (FPWR ALCTL + poll ALSTAT) | Done | With error bit check |
| Synchronous FPRD/FPWR | Done | `ul_ecat_fprd_sync` / `ul_ecat_fpwr_sync` |
| Datagram queue + batch TX/RX | Done | Periodic worker thread |
| ESI XML parser (identity only) | Partial | Extracts vendor/product/rev/serial; no SM/PDO/mailbox parse |
| Python CLI (REPL + batch) | Done | `scripts/ul_ecat_tool.py`; PREEMPT_RT detection |
| Datagram types in enum | Partial | APRD, APWR, FPRD, FPWR, BRD, BWR, LRD, LWR, LRW defined; only APWR/FPRD/FPWR used |
| Distributed Clocks | Sketch | Reads DCSYS0, compares with host clock, writes offset; no propagation delay, no SYNC0/SYNC1 |
| SII / EEPROM read | Not yet | |
| Slave configuration FSM | Not yet | No auto-config sequence (SII -> SM -> FMMU -> state transitions) |
| SM / FMMU configuration | Not yet | |
| Mailbox client | Not yet | |
| CoE / SDO client | Not yet | |
| Cyclic PDO (LRW) | Not yet | |
| Topology detection (DL status) | Not yet | |
| ARMW / FRMW datagram types | Not yet | Not in enum |
| Hot-connect / redundancy | Not yet | |
| FoE / EoE | Not yet | |

## Conformance testing

The EtherCAT Conformance Test Tool (CTT) from the EtherCAT Technology Group is the official tool for validating slave devices. This stack does not yet pass CTT — the features marked "Not yet" above are prerequisites. The current implementation is suitable for prototyping and integration testing with the included simulators.
