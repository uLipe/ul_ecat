/**
 * @file hal_nuttx.h
 * @brief NuttX SPI binding for LAN9252 (see hal_nuttx.c).
 */

#ifndef LAN9252_HAL_NUTTX_H
#define LAN9252_HAL_NUTTX_H

#include <nuttx/spi/spi.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bind LAN9252 HAL to a NuttX SPI device (typically SPI0: @p devid often 0).
 * Call before @ref lan9252_init.
 */
int lan9252_hal_nuttx_init(struct spi_dev_s *spi, uint32_t devid);

#ifdef __cplusplus
}
#endif

#endif
