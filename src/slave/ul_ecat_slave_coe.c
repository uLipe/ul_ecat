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

/* Active segmented-upload transfer state. The slave handles one transfer at a
 * time; a new init request implicitly cancels any in-progress one. */
static struct {
    int                       active;
    const ul_ecat_od_entry_t *entry;
    uint16_t                  offset;
    uint8_t                   toggle_expected;  /* next request toggle bit (0 or UL_ECAT_SDO_BIT_TOGGLE) */
} g_upload;

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
        g_upload.active = 0;
        return build_abort(req, reply, reply_cap, index, subindex, code);
    }
    if ((entry->flags & UL_ECAT_OD_FLAG_R) == 0u) {
        g_upload.active = 0;
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_READ_WO);
    }
    if (entry->length == 0u) {
        g_upload.active = 0;
        return build_abort(req, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
    }
    if (reply_cap < COE_FRAME_HDR_TOTAL) {
        return 0;
    }

    build_reply_headers(req, reply, SDO_HDR_LEN);
    uint8_t *sdo = reply + MBX_HDR_LEN + COE_HDR_LEN;
    w16le(sdo + 1, index);
    sdo[3] = subindex;
    sdo[4] = sdo[5] = sdo[6] = sdo[7] = 0u;

    if (entry->length <= 4u) {
        /* Expedited init upload response: payload in bytes 4..7. */
        g_upload.active = 0;
        uint8_t n = (uint8_t)((4u - entry->length) & 0x03u);
        sdo[0] = UL_ECAT_SDO_SCS_UPLOAD_RESP_INIT |
                 UL_ECAT_SDO_BIT_EXPEDITED |
                 UL_ECAT_SDO_BIT_SIZE_IND |
                 (uint8_t)(n << 2);
        (void)ul_ecat_od_read(entry, sdo + 4, entry->length);
    } else {
        /* Segmented init upload response: total length in bytes 4..7. */
        sdo[0] = UL_ECAT_SDO_SCS_UPLOAD_RESP_INIT | UL_ECAT_SDO_BIT_SIZE_IND;
        sdo[4] = (uint8_t)(entry->length & 0xFFu);
        sdo[5] = (uint8_t)((entry->length >> 8) & 0xFFu);
        sdo[6] = sdo[7] = 0u;
        g_upload.active = 1;
        g_upload.entry = entry;
        g_upload.offset = 0u;
        g_upload.toggle_expected = 0u;  /* first segment carries toggle=0 */
    }
    return COE_FRAME_HDR_TOTAL;
}

static size_t handle_upload_segment(const uint8_t *req, uint8_t *reply, size_t reply_cap,
                                    uint8_t cmd_byte)
{
    if (!g_upload.active || g_upload.entry == NULL) {
        return build_abort(req, reply, reply_cap, 0u, 0u, UL_ECAT_SDO_ABORT_GENERAL);
    }
    uint8_t toggle_in = cmd_byte & UL_ECAT_SDO_BIT_TOGGLE;
    if (toggle_in != g_upload.toggle_expected) {
        g_upload.active = 0;
        return build_abort(req, reply, reply_cap, g_upload.entry->index,
                           g_upload.entry->subindex, UL_ECAT_SDO_ABORT_TOGGLE_BIT);
    }

    if (reply_cap < COE_FRAME_HDR_TOTAL) {
        return 0;
    }
    build_reply_headers(req, reply, SDO_HDR_LEN);
    uint8_t *sdo = reply + MBX_HDR_LEN + COE_HDR_LEN;

    uint16_t remaining = (uint16_t)(g_upload.entry->length - g_upload.offset);
    uint16_t chunk = remaining > 7u ? 7u : remaining;
    uint8_t n_unused = (uint8_t)(7u - chunk);
    uint8_t last = (chunk == remaining) ? UL_ECAT_SDO_BIT_LAST_SEG : 0u;

    sdo[0] = UL_ECAT_SDO_SCS_UPLOAD_SEG_RESP | toggle_in |
             (uint8_t)(n_unused << 1) | last;
    sdo[1] = sdo[2] = sdo[3] = sdo[4] = sdo[5] = sdo[6] = sdo[7] = 0u;

    if (chunk > 0u) {
        const uint8_t *src = (const uint8_t *)g_upload.entry->storage + g_upload.offset;
        memcpy(sdo + 1, src, chunk);
    }

    g_upload.offset = (uint16_t)(g_upload.offset + chunk);
    g_upload.toggle_expected ^= UL_ECAT_SDO_BIT_TOGGLE;
    if (last) {
        g_upload.active = 0;
    }
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

/* SDO Info: Get OD List request -> response. Single-frame only (no fragmentation):
 * if the list does not fit in the reply buffer, indices are truncated and the
 * "incomplete" field stays at 0. Subsequent fragmented requests are not yet
 * supported and would need transfer state akin to upload segmented. */
static size_t handle_sdo_info(const uint8_t *frame, uint8_t *reply, size_t reply_cap,
                              const uint8_t *info_req)
{
    uint8_t opcode = info_req[0];
    if (opcode != UL_ECAT_SDO_INFO_OP_LIST_REQ) {
        return 0;  /* unsupported SDO Info opcode -> drop silently */
    }
    if (reply_cap < MBX_HDR_LEN + COE_HDR_LEN + 8u) {
        return 0;
    }
    uint16_t list_type = (uint16_t)info_req[4] | ((uint16_t)info_req[5] << 8);

    /* Only LIST_ALL is implemented; other types reply with an empty list. */

    /* Build response into the reply buffer directly. */
    uint8_t *info_resp = reply + MBX_HDR_LEN + COE_HDR_LEN;
    info_resp[0] = UL_ECAT_SDO_INFO_OP_LIST_RESP;
    info_resp[1] = 0;
    info_resp[2] = info_resp[3] = 0;            /* fragments left = 0 */
    info_resp[4] = (uint8_t)(list_type & 0xFFu);
    info_resp[5] = (uint8_t)((list_type >> 8) & 0xFFu);

    size_t out_off = 6u;
    uint16_t indices_added = 0u;
    if (list_type == UL_ECAT_SDO_INFO_LIST_ALL) {
        const ul_ecat_od_entry_t *e0 = ul_ecat_od_first();
        size_t n = ul_ecat_od_entries_count();
        uint16_t prev_index = 0xFFFFu;
        for (size_t i = 0; i < n; i++) {
            uint16_t idx = e0[i].index;
            if (idx == prev_index) {
                continue;  /* dedupe (same index, multiple subindexes) */
            }
            prev_index = idx;
            if (MBX_HDR_LEN + COE_HDR_LEN + out_off + 2u > reply_cap) {
                break;
            }
            info_resp[out_off++] = (uint8_t)(idx & 0xFFu);
            info_resp[out_off++] = (uint8_t)((idx >> 8) & 0xFFu);
            indices_added++;
        }
    }

    uint16_t body_len = (uint16_t)out_off;
    /* Mailbox header: copy then patch length (CoE service = SDO Info). */
    memcpy(reply, frame, MBX_HDR_LEN);
    w16le(reply, (uint16_t)(COE_HDR_LEN + body_len));
    reply[MBX_HDR_LEN]     = 0x00u;
    reply[MBX_HDR_LEN + 1] = (uint8_t)(UL_ECAT_COE_SVC_SDO_INFO << 4);
    (void)indices_added;
    return MBX_HDR_LEN + COE_HDR_LEN + body_len;
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
    uint8_t svc = coe_service(coe);
    if (svc == UL_ECAT_COE_SVC_SDO_INFO) {
        return handle_sdo_info(frame, reply, reply_cap, frame + MBX_HDR_LEN + COE_HDR_LEN);
    }
    if (svc != UL_ECAT_COE_SVC_SDO_REQ) {
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
    if (cmd_bits == UL_ECAT_SDO_CCS_UPLOAD_SEG_REQ) {
        return handle_upload_segment(frame, reply, reply_cap, sdo[0]);
    }
    if (cmd_bits == UL_ECAT_SDO_CMD_ABORT) {
        g_upload.active = 0;
        return 0;  /* aborts have no response */
    }
    return build_abort(frame, reply, reply_cap, index, subindex, UL_ECAT_SDO_ABORT_GENERAL);
}
