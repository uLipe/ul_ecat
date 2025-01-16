# Mental model

## Ethernet frame

`[ DA(6) | SA(6) | EtherType 0x88A4 | EtherCAT PDU ]`

## EtherCAT PDU

`[ length(2, LE) | reserved(2, LE) | datagrams… ]`

The **length** field counts the **reserved** two bytes plus all **datagram** octets.

## Datagram

`[ cmd(1) | idx(1) | ADP(2, LE) | ADO(2, LE) | length(2, LE) | irq(2, LE) | data(N) | WKC(2, LE) ]`

The **length** word uses bits **0–10** as the **data length** in bytes.

- **ADP**: address / auto-increment position depending on command.
- **ADO**: ESC register offset (byte address in slave memory map).

## Working counter (WKC)

Each slave that processes the datagram increments WKC. The master checks WKC to decide whether the exchange was valid.

## AL states

`AL Control` at `0x0120` and `AL Status` at `0x0130` (2 bytes, little-endian bit fields). Bits **0–3** carry the requested state (INIT / PREOP / BOOT / SAFEOP / OP). Bit **4** of **AL Control** is the **Acknowledge** bit: after the slave reports the new state in **AL Status**, the master writes the same requested state with **ACK=1** to complete the handshake (see `ul_ecat_al.h`). **AL Status** bit **4** indicates an error condition when set; helpers mask the state nibble for comparison.

## Distributed clocks (optional)

When enabled, the stack queues an **FPRD** of **System Time** at `0x0910` (8 bytes) and compares it to `CLOCK_MONOTONIC` on the host. This is a minimal teaching example, not a full DC setup.
