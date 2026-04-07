/**
 * @file ul_ecat_slave.c
 * @brief Slave state init and Ethernet wrapper around PDU processing.
 */

#include "ul_ecat_slave.h"

#include <string.h>

#include "ul_ecat_esc.h"
#include "ul_ecat_frame.h"

void ul_ecat_slave_init(ul_ecat_slave_t *s, const uint8_t mac[6], const ul_ecat_slave_identity_t *id)
{
    memset(s, 0, sizeof(*s));
    memcpy(s->mac, mac, 6);
    s->logical_position = 0;
    ul_ecat_esc_apply_identity(s->esc, id);
    ul_ecat_esc_set_al_status_init(s->esc);
}

void ul_ecat_slave_reset(ul_ecat_slave_t *s, const ul_ecat_slave_identity_t *id)
{
    memset(s->esc, 0, sizeof(s->esc));
    ul_ecat_esc_apply_identity(s->esc, id);
    ul_ecat_esc_set_al_status_init(s->esc);
}

int ul_ecat_slave_process_ethernet(ul_ecat_slave_t *s,
                                   const uint8_t *rx, size_t rx_len,
                                   uint8_t *tx, size_t tx_cap, size_t *tx_len)
{
    const uint8_t *pdu = NULL;
    size_t pdu_len = 0;

    if (ul_ecat_parse_eth_frame(rx, rx_len, &pdu, &pdu_len) != 0) {
        return -1;
    }

    uint8_t pdu_buf[UL_ECAT_MAX_EC_PAYLOAD];
    if (pdu_len > sizeof(pdu_buf)) {
        return -1;
    }

    size_t out_len = 0;
    if (ul_ecat_slave_process_pdu(s, pdu, pdu_len, pdu_buf, sizeof(pdu_buf), &out_len) != 0) {
        return -1;
    }

    uint8_t master_mac[6];
    memcpy(master_mac, rx + 6, 6);

    ssize_t flen = ul_ecat_build_eth_frame(master_mac, s->mac, pdu_buf, out_len, tx, tx_cap);
    if (flen < 0) {
        return -1;
    }
    *tx_len = (size_t)flen;
    return 0;
}
