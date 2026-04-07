/*
 * Copyright (c) ul_ecat contributors
 * SPDX-License-Identifier: MIT
 *
 * Scan the EtherCAT bus and print discovered slaves (printk).
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "ul_ecat_master.h"

/* Set at build time if needed: -DUL_ECAT_SAMPLE_IFACE_NAME=\"eth1\" */
#ifndef UL_ECAT_SAMPLE_IFACE_NAME
#define UL_ECAT_SAMPLE_IFACE_NAME "eth0"
#endif

int main(void)
{
	static const char iface[] = UL_ECAT_SAMPLE_IFACE_NAME;

	printk("ul_ecat_scan: interface \"%s\"\n", iface);

	ul_ecat_master_settings_t cfg = {
		.iface_name = iface,
		.dst_mac = {0},
		.src_mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
		.rt_priority = 0,
		.dc_enable = 0,
		.dc_sync0_cycle = 1000000u,
	};

	ul_ecat_mac_broadcast(cfg.dst_mac);

	if (ul_ecat_master_init(&cfg) != 0) {
		printk("ul_ecat_scan: ul_ecat_master_init failed\n");
		return 0;
	}

	(void)ul_ecat_scan_network();

	ul_ecat_master_slaves_t *db = ul_ecat_get_master_slaves();

	printk("ul_ecat_scan: %d slave(s)\n", db->slave_count);
	for (int i = 0; i < db->slave_count; i++) {
		const ul_ecat_slave_t *sl = &db->slaves[i];

		printk("  [%d] station=0x%04X vendor=0x%08X product=0x%08X rev=0x%08X ser=0x%08X\n", i,
		       sl->station_address, sl->vendor_id, sl->product_code, sl->revision_no,
		       sl->serial_no);
	}

	(void)ul_ecat_master_shutdown();
	return 0;
}
