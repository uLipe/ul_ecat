/****************************************************************************
 * ul_ecat_scan_main.c
 *
 * SPDX-License-Identifier: MIT
 *
 * NuttX application: EtherCAT scan (same flow as samples/zephyr/ul_ecat_scan).
 ****************************************************************************/

#include <stdio.h>

#include "ul_ecat_master.h"

#ifndef UL_ECAT_SAMPLE_IFACE_NAME
#define UL_ECAT_SAMPLE_IFACE_NAME "eth0"
#endif

int main(int argc, char *argv[])
{
    const char *iface = UL_ECAT_SAMPLE_IFACE_NAME;

    if (argc > 1) {
        iface = argv[1];
    }

    printf("ul_ecat_scan: interface \"%s\"\n", iface);

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
        printf("ul_ecat_scan: ul_ecat_master_init failed\n");
        return 1;
    }

    (void)ul_ecat_scan_network();

    ul_ecat_master_slaves_t *db = ul_ecat_get_master_slaves();

    printf("ul_ecat_scan: %d slave(s)\n", db->slave_count);
    for (int i = 0; i < db->slave_count; i++) {
        const ul_ecat_slave_t *sl = &db->slaves[i];

        printf("  [%d] station=0x%04X vendor=0x%08X product=0x%08X rev=0x%08X ser=0x%08X\n", i,
               sl->station_address, sl->vendor_id, sl->product_code, sl->revision_no,
               sl->serial_no);
    }

    (void)ul_ecat_master_shutdown();
    return 0;
}
