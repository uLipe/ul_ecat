/**
 * @file hal_nuttx.c
 * @brief LAN9252 SPI HAL for NuttX — manual CS between split write/read (matches lan9252.c).
 *
 * Call @ref lan9252_hal_nuttx_init before @ref lan9252_init. Samples use SPI bus 0 by default
 * (pass the board’s `spi_dev_s *` for SPI0).
 */

#include <nuttx/arch.h>
#include <nuttx/spi/spi.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "lan9252_hal.h"

static struct spi_dev_s *g_spidev;
static uint32_t g_devid;
static int g_inited;

int lan9252_hal_nuttx_init(struct spi_dev_s *spi, uint32_t devid)
{
	if (spi == NULL) {
		return -EINVAL;
	}
	g_spidev = spi;
	g_devid = devid;
	g_inited = 1;
	return 0;
}

int lan9252_hal_spi_select(void)
{
	if (!g_inited || g_spidev == NULL) {
		return -EIO;
	}
	SPI_LOCK(g_spidev, true);
	SPI_SELECT(g_spidev, g_devid, true);
	return 0;
}

void lan9252_hal_spi_deselect(void)
{
	if (g_spidev == NULL) {
		return;
	}
	SPI_SELECT(g_spidev, g_devid, false);
	SPI_LOCK(g_spidev, false);
}

int lan9252_hal_spi_write(const uint8_t *data, size_t len)
{
	if (data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (!g_inited || g_spidev == NULL) {
		return -EIO;
	}
	/* nwords: for 8-bit SPI, one word == one byte */
	SPI_SNDBLOCK(g_spidev, data, len);
	return 0;
}

int lan9252_hal_spi_read(uint8_t *data, size_t len)
{
	if (data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (!g_inited || g_spidev == NULL) {
		return -EIO;
	}
	SPI_RECVBLOCK(g_spidev, data, len);
	return 0;
}

void lan9252_hal_delay_us(uint32_t us)
{
	up_udelay(us);
}
