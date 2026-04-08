/*
 * Copyright (c) ul_ecat contributors
 * SPDX-License-Identifier: MIT
 *
 * Minimal EtherCAT slave loop: LAN9252 poll + fictitious servo PDO (position/velocity/torque).
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "lan9252.h"
#include "ul_ecat_slave_controller.h"
#include "ul_ecat_servo_sample.h"

int main(void)
{
	ul_ecat_slave_t slave;
	ul_ecat_slave_controller_t ctrl;
	ul_ecat_slave_identity_t id;
	uint8_t mac[6] = {0x02, 0xEC, 0x40, 0x70, 0x00, 0x01};
	uint8_t rx_stack[UL_ECAT_SERVO_PDO_BYTES];
	uint8_t tx_stack[UL_ECAT_SERVO_PDO_BYTES];
	ul_ecat_slave_controller_pdram_cfg_t pd = {
	    .in_offset = UL_ECAT_SERVO_RXPDO_OFF,
	    .in_len = UL_ECAT_SERVO_PDO_BYTES,
	    .in_buf = rx_stack,
	    .out_offset = UL_ECAT_SERVO_TXPDO_OFF,
	    .out_len = UL_ECAT_SERVO_PDO_BYTES,
	    .out_buf = tx_stack,
	};

	memset(&slave, 0, sizeof(slave));
	memset(&ctrl, 0, sizeof(ctrl));
	memset(rx_stack, 0, sizeof(rx_stack));
	memset(tx_stack, 0, sizeof(tx_stack));
	ul_ecat_servo_sample_identity(&id);

	/* SYS_INIT in controllers/lan9252/ports/zephyr/hal_zephyr.c binds SPI from /chosen ul-ecat-spi */
	k_sleep(K_MSEC(10));

	if (lan9252_init() != 0) {
		printk("ul_ecat_servo: lan9252_init failed\n");
		return 0;
	}

	if (ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id) != 0) {
		printk("ul_ecat_servo: ul_ecat_slave_controller_init failed\n");
		return 0;
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

		k_sleep(K_MSEC(1));
	}
}
