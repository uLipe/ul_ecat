/**
 * @file lan9252_full_mock.h
 * @brief Test double: in-memory EtherCAT core + PRAM behind lan9252.h API (no SPI).
 */

#ifndef LAN9252_FULL_MOCK_H
#define LAN9252_FULL_MOCK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** EtherCAT register space 0x0000–0x0FFF (same layout as ESC ADO). */
uint8_t *lan9252_mock_esc_core(void);
/** Process RAM 0x1000–0x1000+MOCK_PDRAM_BYTES-1 */
uint8_t *lan9252_mock_pdram(void);

void lan9252_mock_reset(void);

/** Direct CSR window (e.g. 0x0300) for tests that call lan9252_read_csr_u32. */
uint8_t *lan9252_mock_csr_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
