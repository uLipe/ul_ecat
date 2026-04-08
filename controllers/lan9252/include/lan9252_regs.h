/**
 * @file lan9252_regs.h
 * @brief LAN9252 CSR addresses and SPI command bytes (SPI PDI serial mode).
 *
 * Values match the Microchip LAN9252 datasheet and the SOES reference
 * (OpenEtherCAT Society, rt-kernel-lan9252 esc_hw.c). Re-verify against your DS revision.
 */

#ifndef LAN9252_REGS_H
#define LAN9252_REGS_H

#include <stdint.h>

#define LAN9252_BIT(x) (1U << (x))

/* --- SPI opcodes (8-bit serial PDI) --- */
#define LAN9252_SPI_SERIAL_WRITE 0x02u
#define LAN9252_SPI_SERIAL_READ  0x03u
#define LAN9252_SPI_FAST_READ    0x0Bu
#define LAN9252_SPI_FAST_READ_DUMMY_BYTES 1u

/* ADDR_INC mode bit for extended SPI (not used in minimal serial path). */
#define LAN9252_SPI_ADDR_INC LAN9252_BIT(6)

/* --- CSR addresses (16-bit, byte addressing of internal parallel map) --- */
#define LAN9252_CSR_PRAM_RD_FIFO 0x0000u
#define LAN9252_CSR_PRAM_WR_FIFO 0x0020u

#define LAN9252_CSR_ESC_CSR_DATA 0x0300u
#define LAN9252_CSR_ESC_CSR_CMD  0x0304u

#define LAN9252_CSR_PRAM_RD_ADDR_LEN 0x0308u
#define LAN9252_CSR_PRAM_RD_CMD      0x030Cu
#define LAN9252_CSR_PRAM_WR_ADDR_LEN 0x0310u
#define LAN9252_CSR_PRAM_WR_CMD      0x0314u

#define LAN9252_CSR_RESET_CTRL 0x01F8u

/* ESC_CSR_CMD @ 0x304 */
#define LAN9252_ESC_CSR_CMD_BUSY LAN9252_BIT(31)
#define LAN9252_ESC_CSR_CMD_READ  (LAN9252_BIT(31) | LAN9252_BIT(30))
#define LAN9252_ESC_CSR_CMD_WRITE LAN9252_BIT(31)
#define LAN9252_ESC_CSR_CMD_SIZE(n) ((uint32_t)(n) << 16)

/* PRAM command bits */
#define LAN9252_PRAM_CMD_BUSY  LAN9252_BIT(31)
#define LAN9252_PRAM_CMD_ABORT LAN9252_BIT(30)
#define LAN9252_PRAM_CMD_AVAIL LAN9252_BIT(0)
#define LAN9252_PRAM_CMD_CNT(v) (((uint32_t)(v) >> 8) & 0x1Fu)

#define LAN9252_PRAM_SIZE(n) ((uint32_t)(n) << 16)
#define LAN9252_PRAM_ADDR(a) ((uint32_t)(a))

/* RESET_CTRL @ 0x1F8 */
#define LAN9252_RESET_CTRL_RST LAN9252_BIT(6)

/* EtherCAT process RAM starts at logical 0x1000 in ETG addressing. */
#define LAN9252_PDRAM_BASE 0x1000u

#endif /* LAN9252_REGS_H */
