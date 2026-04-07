/**
 * @file ul_ecat_slave_pdu.c
 * @brief Walk input EtherCAT datagrams and build response PDU (WKC, FPRD data).
 */

#include "ul_ecat_frame.h"

#include <string.h>

#include "ul_ecat_esc.h"
#include "ul_ecat_slave.h"

#define DG_DATA_MAX 256u

static int process_one(ul_ecat_slave_t *s, uint8_t cmd, uint16_t adp, uint16_t ado, uint16_t dlen,
                       const uint8_t *data_in, uint8_t *data_out, uint16_t *wkc_out)
{
    uint16_t wkc = 0;

    switch (cmd) {
    case UL_ECAT_CMD_FPRD: {
        uint16_t st = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_STADR);
        if (st != 0u && adp == st) {
            if (dlen > 0u && ul_ecat_esc_read(s->esc, ado, data_out, dlen) == 0) {
                wkc = 1u;
            }
        }
        break;
    }
    case UL_ECAT_CMD_FPWR: {
        uint16_t st = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_STADR);
        if (st != 0u && adp == st) {
            if (dlen > 0u && data_in != NULL &&
                ul_ecat_esc_write(s->esc, ado, data_in, dlen) == 0) {
                wkc = 1u;
            }
        }
        break;
    }
    case UL_ECAT_CMD_APWR: {
        if (adp == s->logical_position && dlen > 0u && data_in != NULL &&
            ul_ecat_esc_write(s->esc, ado, data_in, dlen) == 0) {
            wkc = 1u;
        }
        break;
    }
    default:
        break;
    }
    *wkc_out = wkc;
    return 0;
}

int ul_ecat_slave_process_pdu(ul_ecat_slave_t *s,
                              const uint8_t *pdu_in, size_t pdu_in_len,
                              uint8_t *pdu_out, size_t pdu_out_cap, size_t *pdu_out_len)
{
    int nd = ul_ecat_pdu_count_datagrams(pdu_in, pdu_in_len);
    if (nd < 0) {
        return -1;
    }

    size_t off_in = 0;
    size_t off_out = 0;

    for (int i = 0; i < nd; i++) {
        uint8_t cmd = 0;
        uint8_t idx = 0;
        uint16_t adp = 0;
        uint16_t ado = 0;
        uint16_t dlen = 0;
        uint16_t irq = 0;
        uint16_t wkc_in = 0;
        uint8_t datab[DG_DATA_MAX];

        if (ul_ecat_dgram_parse(pdu_in, pdu_in_len, (unsigned)i, &cmd, &idx, &adp, &ado, &dlen, &irq,
                                &wkc_in, datab, sizeof(datab)) != 0) {
            return -1;
        }
        if (dlen > DG_DATA_MAX) {
            return -1;
        }

        const uint8_t *din = (dlen > 0u) ? datab : NULL;
        uint16_t wkc = 0;
        (void)process_one(s, cmd, adp, ado, dlen, din, datab, &wkc);

        int enc = ul_ecat_dgram_encode(pdu_out + off_out, pdu_out_cap - off_out, cmd, idx, adp, ado, dlen,
                                       irq, wkc, datab);
        if (enc < 0) {
            return -1;
        }
        off_out += (size_t)enc;
    }

    *pdu_out_len = off_out;
    return 0;
}
