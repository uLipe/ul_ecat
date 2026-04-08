/****************************************************************************
 * ul_ecat_servo_main.c
 *
 * SPDX-License-Identifier: MIT
 *
 * EtherCAT slave + LAN9252: bind SPI0 via board hook, then controller poll + PDO.
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/spi/spi.h>

#include "lan9252.h"
#include "lan9252_hal_nuttx.h"
#include "ul_ecat_slave_controller.h"
#include "ul_ecat_servo_sample.h"

/**
 * Return the NuttX SPI device for LAN9252 (typically SPI0 / devid 0).
 * Provide a strong definition in your board code or app (see README).
 */
__attribute__((weak)) struct spi_dev_s *board_ul_ecat_lan9252_spi0(void)
{
	return NULL;
}

int main(int argc, char *argv[])
{
	ul_ecat_slave_t slave;
	ul_ecat_slave_controller_t ctrl;
	ul_ecat_slave_identity_t id;
	uint8_t mac[6] = {0x02, 0xEC, 0x40, 0x70, 0x00, 0x01};
	uint8_t rx_stack[UL_ECAT_SERVO_PDO_BYTES];
	uint8_t tx_stack[UL_ECAT_SERVO_PDO_BYTES];
	ul_ecat_slave_controller_pdram_cfg_t pd;
	struct spi_dev_s *spi;

	(void)argc;
	(void)argv;

	memset(&slave, 0, sizeof(slave));
	memset(&ctrl, 0, sizeof(ctrl));
	memset(rx_stack, 0, sizeof(rx_stack));
	memset(tx_stack, 0, sizeof(tx_stack));
	ul_ecat_servo_sample_identity(&id);

	pd.in_offset = UL_ECAT_SERVO_RXPDO_OFF;
	pd.in_len = UL_ECAT_SERVO_PDO_BYTES;
	pd.in_buf = rx_stack;
	pd.out_offset = UL_ECAT_SERVO_TXPDO_OFF;
	pd.out_len = UL_ECAT_SERVO_PDO_BYTES;
	pd.out_buf = tx_stack;

	spi = board_ul_ecat_lan9252_spi0();
	if (spi == NULL) {
		printf("ul_ecat_servo: implement board_ul_ecat_lan9252_spi0() returning SPI0 (see README)\n");
		return 1;
	}

	if (lan9252_hal_nuttx_init(spi, 0U) != 0) {
		printf("ul_ecat_servo: lan9252_hal_nuttx_init failed\n");
		return 1;
	}

	if (lan9252_init() != 0) {
		printf("ul_ecat_servo: lan9252_init failed\n");
		return 1;
	}

	if (ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id) != 0) {
		printf("ul_ecat_servo: ul_ecat_slave_controller_init failed\n");
		return 1;
	}

	ul_ecat_slave_controller_set_pdram(&ctrl, &pd);

	for (;;) {
		(void)ul_ecat_slave_controller_poll(&ctrl, 0);

		{
			ul_ecat_servo_txpdo_t *tx = (void *)tx_stack;
			const ul_ecat_servo_rxpdo_t *rx = (const void *)rx_stack;

			tx->actual_position = rx->target_position;
			tx->actual_velocity = rx->target_velocity;
			tx->actual_torque = rx->target_torque;
		}

		usleep(1000);
	}

	return 0;
}
