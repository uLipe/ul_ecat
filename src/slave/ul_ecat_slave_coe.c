/**
 * @file ul_ecat_slave_coe.c
 * @brief CoE / SDO server (expedited upload + download).
 */

#include "ul_ecat_slave_coe.h"

#include "ul_ecat_esc_regs.h"
#include "ul_ecat_slave_od.h"

#include <string.h>

#define MBX_HDR_LEN  UL_ECAT_MBX_HDR_LEN
#define COE_HDR_LEN  2u
#define SDO_HDR_LEN  8u
#define COE_FRAME_HDR_TOTAL (MBX_HDR_LEN + COE_HDR_LEN + SDO_HDR_LEN)

static uint16_t r16le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static void w16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
static void w32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint8_t coe_service(const uint8_t *coe_hdr)
{
    /* coe_hdr[1] high nibble = service */
    return (uint8_t)((coe_hdr[1] >> 4) & 0x0Fu);
}

/* Copy mailbox + CoE headers from the request into the reply, preserving
 * address/channel/priority and incrementing nothing (slave responds with the
 * same counter the master used; ETG slaves typically echo it). The CoE service
 * is set to SDO Response and the body length is set by the caller. */
static void build_reply_headers(const uint8_t *req, uint8_t *reply, uint16_t body_len)
{
    /* Mailbox header: copy then patch length. */
    memcpy(reply, req, MBX_HDR_LEN);
    uint16_t mbx_len = (uint16_t)(COE_HDR_LEN + body_len);
    w16le(reply, mbx_len);
    /* CoE header: number=0, service=SDO Resp. */
    reply[MBX_HDR_LEN]     = 0x00u;
    reply[MBX_HDR_LEN + 1] = (uint8_t)(UL_ECAT_COE_SVC_SDO_RESP << 4);
}

static size_t build_abort(const uint8_t *req, uint8_t *reply, size_t reply_cap,
                          uint16_t index, uint8_t subindex, uint32_t code)
{
    if (reply_cap < COE_FRAME_HDR_TOTAL) {
        return 0;
    }
    build_reply_headers(req, reply, SDO_HDR_LEN);
    uint8_t *sdo = reply + MBX_HDR_LEN + COE_HDR_LEN;
    sdo[0] = UL_ECAT_SDO_CMD_ABORT;
    w16le(sdo + 1, index);
    sdo[3] = subindex;
    w32le(sdo + 4, code);
    return COE_FRAME_HDR_TOTAL;
}

static size_t handle_upload_init(const uint8_t *req, uint8_t *reply, size_t reply_cap,
                                 uint16_t index, uint8_t subindex)
{
    const ul_ecat_od_entry_t *entry = ul_ecat_od_lookup(index, subindex);
    if (entry == NULL) {
        uint32_t code = ul_ecat_od_index_exists(index)
                            ? UL_ECAT_SDO_ABORT_BAD_SUBINDEX
                            : UL_ECAT_SDO_ABORT_OBJECT_GONE;
        return build_abort(req, reply, reply_cap, index, subindex, code);
    }
    if ((entry->flags & UL_ECAT_OD_FLAG_R) == 0u) {
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_READ_WO);
    }

    /* Expedited only in 4c: payload must fit 4 bytes. Larger objects are
     * answered with abort GENERAL until 4d adds segmented upload. */
    if (entry->length == 0u || entry->length > 4u) {
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
    }
    if (reply_cap < COE_FRAME_HDR_TOTAL) {
        return 0;
    }

    build_reply_headers(req, reply, SDO_HDR_LEN);
    uint8_t *sdo = reply + MBX_HDR_LEN + COE_HDR_LEN;

    uint8_t n = (uint8_t)((4u - entry->length) & 0x03u);  /* unused bytes count */
    sdo[0] = UL_ECAT_SDO_SCS_UPLOAD_RESP_INIT |
             UL_ECAT_SDO_BIT_EXPEDITED |
             UL_ECAT_SDO_BIT_SIZE_IND |
             (uint8_t)(n << 2);
    w16le(sdo + 1, index);
    sdo[3] = subindex;
    sdo[4] = sdo[5] = sdo[6] = sdo[7] = 0u;
    (void)ul_ecat_od_read(entry, sdo + 4, entry->length);
    return COE_FRAME_HDR_TOTAL;
}

static size_t handle_download_init(const uint8_t *req, uint8_t *reply, size_t reply_cap,
                                   const uint8_t *sdo_req, uint16_t index, uint8_t subindex)
{
    const ul_ecat_od_entry_t *entry = ul_ecat_od_lookup(index, subindex);
    if (entry == NULL) {
        uint32_t code = ul_ecat_od_index_exists(index)
                            ? UL_ECAT_SDO_ABORT_BAD_SUBINDEX
                            : UL_ECAT_SDO_ABORT_OBJECT_GONE;
        return build_abort(req, reply, reply_cap, index, subindex, code);
    }
    if ((entry->flags & UL_ECAT_OD_FLAG_W) == 0u) {
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_WRITE_RO);
    }

    uint8_t cmd = sdo_req[0];
    if ((cmd & UL_ECAT_SDO_BIT_EXPEDITED) == 0u) {
        /* Segmented download not implemented yet (4d). */
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
    }
    uint8_t n = (cmd & UL_ECAT_SDO_BIT_SIZE_IND) ? (uint8_t)((cmd >> 2) & 0x03u) : 0u;
    uint8_t payload_len = (uint8_t)(4u - n);
    if (payload_len == 0u || payload_len > entry->length) {
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_LENGTH_MM);
    }
    if (ul_ecat_od_write(entry, sdo_req + 4, payload_len) != UL_ECAT_OD_OK) {
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
    }

    if (reply_cap < COE_FRAME_HDR_TOTAL) {
        return 0;
    }
    build_reply_headers(req, reply, SDO_HDR_LEN);
    uint8_t *sdo = reply + MBX_HDR_LEN + COE_HDR_LEN;
    sdo[0] = UL_ECAT_SDO_SCS_DOWNLOAD_RESP_INIT;
    w16le(sdo + 1, index);
    sdo[3] = subindex;
    sdo[4] = sdo[5] = sdo[6] = sdo[7] = 0u;
    return COE_FRAME_HDR_TOTAL;
}

size_t ul_ecat_slave_coe_process(const uint8_t *frame, size_t len,
                                 uint8_t *reply, size_t reply_cap)
{
    if (frame == NULL || reply == NULL || len < COE_FRAME_HDR_TOTAL) {
        return 0;
    }
    /* Mailbox type must be CoE. */
    uint8_t mbx_type = frame[5] & 0x0Fu;
    if (mbx_type != UL_ECAT_MBX_TYPE_COE) {
        return 0;
    }
    const uint8_t *coe = frame + MBX_HDR_LEN;
    if (coe_service(coe) != UL_ECAT_COE_SVC_SDO_REQ) {
        return 0;
    }
    const uint8_t *sdo = frame + MBX_HDR_LEN + COE_HDR_LEN;
    uint8_t cmd_bits = sdo[0] & 0xE0u;
    uint16_t index = r16le(sdo + 1);
    uint8_t subindex = sdo[3];

    if (cmd_bits == UL_ECAT_SDO_CCS_UPLOAD_REQ_INIT) {
        return handle_upload_init(frame, reply, reply_cap, index, subindex);
    }
    if (cmd_bits == UL_ECAT_SDO_CCS_DOWNLOAD_REQ_INIT) {
        return handle_download_init(frame, reply, reply_cap, sdo, index, subindex);
    }
    return build_abort(frame, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
}
