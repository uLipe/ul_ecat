# LAN9252 datasheet reference

The EtherCAT Slave Controller **LAN9252** (Microchip) full **datasheet** and **application notes** should be obtained from:

- [Microchip LAN9252 product page](https://www.microchip.com/en-us/product/LAN9252)

Register addresses and SPI command bytes used in [`controllers/lan9252`](../controllers/lan9252/) were cross-checked against the **open SOES** reference (`soes/hal/rt-kernel-lan9252/esc_hw.c`, OpenEtherCAT Society), which tracks Microchip’s documented CSR map (indirect EtherCAT core access at `0x300` / `0x304`, PRAM FIFO at `0x000` / `0x020`, command regs at `0x308`–`0x314`, reset at `0x1F8`).

If you add a local PDF under `doc/`, extend this file with the exact document number (e.g. DS-xxxx) for traceability.
