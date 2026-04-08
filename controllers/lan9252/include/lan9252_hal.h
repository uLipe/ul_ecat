/**
 * @file lan9252_hal.h
 * @brief OS / board SPI hooks for LAN9252 (implement on Zephyr, NuttX, bare-metal).
 *
 * The driver calls these with CS deasserted between select/deselect pairs unless noted.
 * Implementations may use SPI controller hardware CS or GPIO.
 */

#ifndef LAN9252_HAL_H
#define LAN9252_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Assert chip-select (active low typical) before a transaction sequence.
 * @return 0 on success, negative on error (e.g. -EIO).
 */
int lan9252_hal_spi_select(void);

/** Release chip-select after the transaction sequence. */
void lan9252_hal_spi_deselect(void);

/**
 * Half-duplex MOSI write (MISO ignored).
 * @return 0 on success, negative on error.
 */
int lan9252_hal_spi_write(const uint8_t *data, size_t len);

/**
 * Half-duplex MISO read: clocks out @p len bytes on MOSI as 0x00 while sampling MISO.
 * @return 0 on success, negative on error.
 */
int lan9252_hal_spi_read(uint8_t *data, size_t len);

/**
 * Optional microsecond delay (reset / startup spacing). Weak default is empty.
 */
void lan9252_hal_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* LAN9252_HAL_H */
