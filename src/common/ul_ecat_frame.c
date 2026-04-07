/**
 * @file ul_ecat_frame.c
 * @brief EtherCAT PDU/datagram wire encode and decode (little-endian fields per ETG).
 */

#include "ul_ecat_frame.h"

#include <string.h>

static void w16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static uint16_t r16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint16_t ul_ecat_pack_len_irq(uint16_t data_len_bytes, uint16_t irq)
{
    (void)irq;
    if (data_len_bytes > 0x7FFu) {
        data_len_bytes = 0x7FFu;
    }
    return (uint16_t)(data_len_bytes & 0x7FFu);
}

uint16_t ul_ecat_unpack_len(uint16_t len_word)
{
    return (uint16_t)(len_word & 0x7FFu);
}

size_t ul_ecat_dgram_wire_size(uint16_t data_len_bytes)
{
    return (size_t)UL_ECAT_DGRAM_HDR_LEN + (size_t)data_len_bytes + UL_ECAT_DGRAM_WKC_LEN;
}

int ul_ecat_dgram_encode(uint8_t *out, size_t out_cap,
                         uint8_t cmd, uint8_t index,
                         uint16_t adp, uint16_t ado,
                         uint16_t data_len_bytes, uint16_t irq, uint16_t wkc_out,
                         const void *data)
{
    if (data_len_bytes > UL_ECAT_DGRAM_MAX_DATA) {
        return -1;
    }
    size_t need = ul_ecat_dgram_wire_size(data_len_bytes);
    if (need > out_cap) {
        return -1;
    }
    out[0] = cmd;
    out[1] = index;
    w16_le(out + 2, adp);
    w16_le(out + 4, ado);
    w16_le(out + 6, ul_ecat_pack_len_irq(data_len_bytes, irq));
    w16_le(out + 8, irq);
    if (data_len_bytes > 0u && data != NULL) {
        memcpy(out + UL_ECAT_DGRAM_HDR_LEN, data, data_len_bytes);
    } else if (data_len_bytes > 0u && data == NULL) {
        memset(out + UL_ECAT_DGRAM_HDR_LEN, 0, data_len_bytes);
    }
    w16_le(out + UL_ECAT_DGRAM_HDR_LEN + data_len_bytes, wkc_out);
    return (int)need;
}

/** ETG datagram header: cmd, idx, adp, ado, length(11b), irq(16b). */
static int parse_one_dgram(const uint8_t *dg, size_t dg_remain,
                           uint8_t *cmd_out, uint8_t *index_out,
                           uint16_t *adp_out, uint16_t *ado_out,
                           uint16_t *data_len_out, uint16_t *irq_out,
                           uint16_t *wkc_out,
                           void *data_out, size_t data_cap)
{
    if (dg_remain < UL_ECAT_DGRAM_HDR_LEN + UL_ECAT_DGRAM_WKC_LEN) {
        return -1;
    }
    uint8_t cmd = dg[0];
    uint8_t index = dg[1];
    uint16_t adp = r16_le(dg + 2);
    uint16_t ado = r16_le(dg + 4);
    uint16_t len_word = r16_le(dg + 6);
    uint16_t irq = r16_le(dg + 8);
    uint16_t dlen = ul_ecat_unpack_len(len_word);
    if (dlen > UL_ECAT_DGRAM_MAX_DATA) {
        return -1;
    }
    size_t total = ul_ecat_dgram_wire_size(dlen);
    if (dg_remain < total) {
        return -1;
    }
    uint16_t wkc = r16_le(dg + UL_ECAT_DGRAM_HDR_LEN + dlen);
    if (cmd_out) {
        *cmd_out = cmd;
    }
    if (index_out) {
        *index_out = index;
    }
    if (adp_out) {
        *adp_out = adp;
    }
    if (ado_out) {
        *ado_out = ado;
    }
    if (data_len_out) {
        *data_len_out = dlen;
    }
    if (irq_out) {
        *irq_out = irq;
    }
    if (wkc_out) {
        *wkc_out = wkc;
    }
    if (data_out != NULL && dlen > 0u) {
        if (data_cap < dlen) {
            return -1;
        }
        memcpy(data_out, dg + UL_ECAT_DGRAM_HDR_LEN, dlen);
    }
    return (int)total;
}

int ul_ecat_dgram_parse(const uint8_t *pdu, size_t pdu_len, unsigned dgram_index,
                        uint8_t *cmd_out, uint8_t *index_out,
                        uint16_t *adp_out, uint16_t *ado_out,
                        uint16_t *data_len_out, uint16_t *irq_out,
                        uint16_t *wkc_out,
                        void *data_out, size_t data_cap)
{
    size_t off = 0;
    for (unsigned i = 0; i <= dgram_index; i++) {
        if (off + UL_ECAT_DGRAM_HDR_LEN + UL_ECAT_DGRAM_WKC_LEN > pdu_len) {
            return -1;
        }
        const uint8_t *dg = pdu + off;
        size_t remain = pdu_len - off;
        uint16_t len_irq = r16_le(dg + 6);
        uint16_t dlen = ul_ecat_unpack_len(len_irq);
        size_t dtot = ul_ecat_dgram_wire_size(dlen);
        if (dtot > remain) {
            return -1;
        }
        if (i == dgram_index) {
            int pr = parse_one_dgram(dg, remain, cmd_out, index_out, adp_out, ado_out,
                                     data_len_out, irq_out, wkc_out, data_out, data_cap);
            return pr < 0 ? -1 : 0;
        }
        off += dtot;
    }
    return -1;
}

int ul_ecat_pdu_count_datagrams(const uint8_t *pdu, size_t pdu_len)
{
    size_t off = 0;
    int count = 0;
    while (off + UL_ECAT_DGRAM_HDR_LEN + UL_ECAT_DGRAM_WKC_LEN <= pdu_len) {
        const uint8_t *dg = pdu + off;
        uint16_t len_irq = r16_le(dg + 6);
        uint16_t dlen = ul_ecat_unpack_len(len_irq);
        size_t dtot = ul_ecat_dgram_wire_size(dlen);
        if (dtot > pdu_len - off) {
            return -1;
        }
        off += dtot;
        count++;
    }
    if (off != pdu_len) {
        return -1;
    }
    return count;
}

ssize_t ul_ecat_build_eth_frame(const uint8_t dst[6], const uint8_t src[6],
                                const uint8_t *ec_payload, size_t ec_payload_len,
                                uint8_t *frame_out, size_t frame_cap)
{
    /* ec_payload = concatenated datagrams only (no 4-byte ECAT header inside ec_payload). */
    size_t pdu_body = UL_ECAT_PDU_HDR_LEN + ec_payload_len;
    size_t eth_len = 14u + pdu_body;
    if (eth_len > frame_cap) {
        return -1;
    }
    if (ec_payload_len > UL_ECAT_MAX_EC_PAYLOAD) {
        return -1;
    }
    memcpy(frame_out, dst, 6);
    memcpy(frame_out + 6, src, 6);
    frame_out[12] = (uint8_t)((UL_ECAT_ETHERTYPE >> 8) & 0xFFu);
    frame_out[13] = (uint8_t)(UL_ECAT_ETHERTYPE & 0xFFu);
    /* ECAT PDU length field = size of EtherCAT data following this 2-byte field (reserved + datagrams) */
    uint16_t ec_len_field = (uint16_t)(2u + ec_payload_len);
    w16_le(frame_out + 14, ec_len_field);
    w16_le(frame_out + 16, 0u);
    if (ec_payload_len > 0u && ec_payload != NULL) {
        memcpy(frame_out + 18, ec_payload, ec_payload_len);
    }
    return (ssize_t)eth_len;
}

int ul_ecat_parse_eth_frame(const uint8_t *frame, size_t frame_len,
                            const uint8_t **ec_payload_out, size_t *ec_payload_len_out)
{
    if (frame_len < 14u + UL_ECAT_PDU_HDR_LEN) {
        return -1;
    }
    uint16_t et = ((uint16_t)frame[12] << 8) | (uint16_t)frame[13];
    if (et != UL_ECAT_ETHERTYPE) {
        return -1;
    }
    uint16_t ec_len_field = r16_le(frame + 14);
    /* ec_len_field covers reserved(2) + datagrams */
    if (ec_len_field < 2u) {
        return -1;
    }
    size_t dgram_area = (size_t)ec_len_field - 2u;
    size_t expected = 14u + UL_ECAT_PDU_HDR_LEN + dgram_area;
    if (frame_len < expected) {
        return -1;
    }
    if (ec_payload_out) {
        *ec_payload_out = frame + 18;
    }
    if (ec_payload_len_out) {
        *ec_payload_len_out = dgram_area;
    }
    return 0;
}
