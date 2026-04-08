/**
 * @file lan9252.h
 * @brief Minimal LAN9252 SPI driver — CSR, indirect EtherCAT core access, PRAM (PDRAM).
 *
 * The EtherCAT Ethernet path is handled inside the LAN9252; this module only bridges SPI.
 */

#ifndef LAN9252_H
#define LAN9252_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Placeholder for future chip probe / HAL sanity. Currently returns 0.
 */
int lan9252_init(void);

/**
 * Optional soft reset of the EtherCAT core (SPI write to RESET_CTRL).
 * Polls until the reset bit clears. Requires working HAL.
 * @return 0 on success, negative errno-style code on error.
 */
int lan9252_soft_reset(void);

/**
 * Read a 32-bit CSR register (direct SPI fast read @p csr_addr).
 * @return 0 on success.
 */
int lan9252_read_csr_u32(uint16_t csr_addr, uint32_t *out);

/**
 * Write a 32-bit CSR register (direct SPI serial write @p csr_addr).
 * @return 0 on success.
 */
int lan9252_write_csr_u32(uint16_t csr_addr, uint32_t val);

/**
 * Read EtherCAT core register space (ADO &lt; 0x1000) via indirect CSR mechanism.
 * Applies LAN9252 access width rules (1/2/4 bytes per datasheet table 12-14).
 * @return 0 on success.
 */
int lan9252_esc_read(uint16_t ado, void *buf, uint16_t len);

/**
 * Write EtherCAT core register space (ADO &lt; 0x1000) via indirect CSR mechanism.
 * @return 0 on success.
 */
int lan9252_esc_write(uint16_t ado, const void *buf, uint16_t len);

/**
 * Read process data RAM (address in ETG space, typically @p offset &gt;= 0x1000).
 * Uses PRAM read engine + FIFO.
 * @return 0 on success.
 */
int lan9252_pdram_read(uint16_t offset, void *buf, uint16_t len);

/**
 * Write process data RAM (write-side FIFO path).
 * @return 0 on success.
 */
int lan9252_pdram_write(uint16_t offset, const void *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* LAN9252_H */
