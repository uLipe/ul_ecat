/**
 * @file hal_zephyr.c
 * @brief LAN9252 SPI HAL for Zephyr — binds via /chosen ul-ecat-spi (SYS_INIT before main).
 *
 * CS is driven manually so LAN9252 split write/read sequences keep CS asserted (see lan9252.c).
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "lan9252_hal.h"

#if IS_ENABLED(CONFIG_UL_ECAT_LAN9252_HAL)

#if !DT_HAS_CHOSEN(ul_ecat_spi)
#error "CONFIG_UL_ECAT_LAN9252_HAL requires /chosen ul-ecat-spi in devicetree"
#endif

#define UL_ECAT_SPI_NODE DT_CHOSEN(ul_ecat_spi)

/* 8-bit, MSB first, mode 0 — adjust overlay if your LAN9252 strapping differs */
#define UL_ECAT_SPI_OP (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB)

static struct spi_dt_spec g_spi;
static struct gpio_dt_spec g_cs;
static struct spi_config g_cfg_nocs;
static struct k_mutex g_spi_mtx;
static volatile int g_hal_ready;

static void inhibit_cs_in_config(void)
{
	g_cfg_nocs = g_spi.config;
	g_cfg_nocs.cs.gpio.port = NULL;
	g_cfg_nocs.cs.gpio.pin = 0;
	g_cfg_nocs.cs.gpio.dt_flags = 0;
	g_cfg_nocs.cs.delay = 0;
}

static int ul_ecat_lan9252_hal_init(void)
{
	int ret;

	g_spi = (struct spi_dt_spec)SPI_DT_SPEC_GET(UL_ECAT_SPI_NODE, UL_ECAT_SPI_OP, 0);
	if (!spi_is_ready_dt(&g_spi)) {
		return -ENODEV;
	}

	g_cs = g_spi.config.cs.gpio;
	if (g_cs.port == NULL) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&g_cs, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}

	inhibit_cs_in_config();

	k_mutex_init(&g_spi_mtx);
	g_hal_ready = 1;
	return 0;
}

SYS_INIT(ul_ecat_lan9252_hal_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

int lan9252_hal_spi_select(void)
{
	int ret;

	if (!g_hal_ready) {
		return -EIO;
	}

	ret = k_mutex_lock(&g_spi_mtx, K_FOREVER);
	if (ret != 0) {
		return -EIO;
	}

	ret = gpio_pin_set_dt(&g_cs, 0);
	if (ret != 0) {
		(void)k_mutex_unlock(&g_spi_mtx);
		return -EIO;
	}
	return 0;
}

void lan9252_hal_spi_deselect(void)
{
	(void)gpio_pin_set_dt(&g_cs, 1);
	(void)k_mutex_unlock(&g_spi_mtx);
}

int lan9252_hal_spi_write(const uint8_t *data, size_t len)
{
	struct spi_buf txb;
	struct spi_buf_set txs;
	int ret;

	if (data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (!g_hal_ready) {
		return -EIO;
	}

	txb.buf = (void *)data;
	txb.len = len;
	txs.buffers = &txb;
	txs.count = 1U;

	ret = spi_write(g_spi.bus, &g_cfg_nocs, &txs);
	return (ret == 0) ? 0 : -EIO;
}

int lan9252_hal_spi_read(uint8_t *data, size_t len)
{
	struct spi_buf rxb;
	struct spi_buf_set rxs;
	int ret;

	if (data == NULL || len == 0U) {
		return -EINVAL;
	}
	if (!g_hal_ready) {
		return -EIO;
	}

	rxb.buf = data;
	rxb.len = len;
	rxs.buffers = &rxb;
	rxs.count = 1U;

	ret = spi_read(g_spi.bus, &g_cfg_nocs, &rxs);
	return (ret == 0) ? 0 : -EIO;
}

void lan9252_hal_delay_us(uint32_t us)
{
	if (us == 0U) {
		return;
	}
	if (us < 100U) {
		k_busy_wait(us);
	} else {
		k_usleep(us);
	}
}

#endif /* CONFIG_UL_ECAT_LAN9252_HAL */
