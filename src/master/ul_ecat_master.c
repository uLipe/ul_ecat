/**
 * @file ul_ecat_master.c
 * @brief Minimal EtherCAT master: Linux AF_PACKET, ETG-style datagrams, mutex-protected I/O.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "ul_ecat_master.h"
#include "ul_ecat_al.h"
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_frame.h"
#include "ul_ecat_osal.h"
#include "ul_ecat_transport.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Set UL_ECAT_VERBOSE=1 for per-datagram trace during ul_ecat_scan_network(). */
static int g_scan_trace;

#include <arpa/inet.h>

#define CYCLE_TIME_NS 1000000UL
#define Q_MAX 32
#define DGRAM_BUF 512

/* Short aliases for readability (from ul_ecat_esc_regs.h). */
#define ESC_REG_STADR   UL_ECAT_ESC_REG_STADR
#define ESC_REG_ALCTL   UL_ECAT_ESC_REG_ALCTL
#define ESC_REG_ALSTAT  UL_ECAT_ESC_REG_ALSTAT
#define ESC_REG_DCSYS0  UL_ECAT_ESC_REG_DCSYS0
#define ESC_REG_DCSYSOFS UL_ECAT_ESC_REG_DCSYSOFS

static int g_sockfd = -1;
static unsigned char g_dst_mac[6];
static unsigned char g_src_mac[6];
static volatile int g_running = 1;

static int g_dc_state;
static int64_t g_dc_offset;
static int g_dc_timeout;
static int64_t g_dc_threshold_ns = 5000000LL;
static int g_dc_error_code;

static ul_ecat_master_slaves_t g_slaves_db;

static struct {
    uint16_t wire_len;
    uint8_t data[DGRAM_BUF];
} g_q[Q_MAX];
static int g_q_count;
static uint8_t g_dgram_index_seq;

static void w16_le_buf(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static uint16_t r16_le_buf(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

ul_ecat_master_slaves_t *ul_ecat_get_master_slaves(void)
{
    return &g_slaves_db;
}

void ul_ecat_mac_broadcast(unsigned char dst[6])
{
    memset(dst, 0xFF, 6);
}

static void dc_init(void);
static void dc_cycle(void);
static void handle_dc_response(uint64_t slave_time_ns);
static void ul_ecat_eventloop_post_dc_event(const ul_ecat_dc_event_info_t *);
static void ul_ecat_eventloop_post_frame_event(const struct ul_ecat_frame *, ssize_t);

static int fprd_blocking(uint16_t station, uint16_t ado, void *out, uint16_t len,
                         uint16_t *wkc_out, int timeout_ms);
static int fpwr_blocking(uint16_t adp, uint16_t ado, const void *data, uint16_t len,
                         uint16_t *wkc_out, int timeout_ms);

typedef struct {
    ul_ecat_esi_device_t devs[64];
    int dev_count;
} ecat_esi_lib_t;
static ecat_esi_lib_t g_esi_lib;

static int parse_hex_tag(const char *xml, const char *tag, uint32_t *val);
static int parse_string_tag(const char *xml, const char *tag, char *out, size_t out_sz);

int ul_ecat_esi_parse_file(const char *filename, ul_ecat_esi_parse_result_t *out_res)
{
    if (!filename || !out_res) {
        return -1;
    }
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen ESI");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsz <= 0) {
        fclose(fp);
        return -1;
    }
    char *xml = (char *)malloc((size_t)fsz + 1u);
    if (!xml) {
        fclose(fp);
        return -1;
    }
    fread(xml, 1u, (size_t)fsz, fp);
    fclose(fp);
    xml[fsz] = '\0';

    out_res->count = 0;
    char *search = xml;
    for (int i = 0; i < 16; i++) {
        char *dev_open = strstr(search, "<Device>");
        if (!dev_open) {
            break;
        }
        char *dev_close = strstr(dev_open, "</Device>");
        if (!dev_close) {
            break;
        }
        size_t dlen = (size_t)(dev_close - dev_open + 8);
        char temp[4096];
        if (dlen >= sizeof(temp)) {
            dlen = sizeof(temp) - 1u;
        }
        strncpy(temp, dev_open, dlen);
        temp[dlen] = '\0';

        ul_ecat_esi_device_t *dev = &out_res->devices[out_res->count];
        memset(dev, 0, sizeof(*dev));

        parse_hex_tag(temp, "VendorID", &dev->vendor_id);
        parse_hex_tag(temp, "ProductCode", &dev->product_code);
        parse_hex_tag(temp, "RevisionNo", &dev->revision_no);
        parse_hex_tag(temp, "SerialNo", &dev->serial_no);
        parse_string_tag(temp, "Name", dev->device_name, sizeof(dev->device_name));

        out_res->count++;
        if (out_res->count >= 16) {
            break;
        }
        search = dev_close + 8;
    }
    free(xml);
    return 0;
}

static void merge_esi_res(const ul_ecat_esi_parse_result_t *res)
{
    for (int i = 0; i < res->count; i++) {
        if (g_esi_lib.dev_count >= 64) {
            break;
        }
        g_esi_lib.devs[g_esi_lib.dev_count] = res->devices[i];
        g_esi_lib.dev_count++;
    }
}

static int parse_hex_tag(const char *xml, const char *tag, uint32_t *val)
{
    char ot[64], ct[64];
    snprintf(ot, sizeof(ot), "<%s>", tag);
    snprintf(ct, sizeof(ct), "</%s>", tag);
    char *s = strstr(xml, ot);
    if (!s) {
        return -1;
    }
    s += strlen(ot);
    char *e = strstr(s, ct);
    if (!e) {
        return -1;
    }
    char tmp[128];
    size_t l = (size_t)(e - s);
    if (l >= sizeof(tmp)) {
        l = sizeof(tmp) - 1u;
    }
    strncpy(tmp, s, l);
    tmp[l] = '\0';
    *val = (uint32_t)strtoul(tmp, NULL, 16);
    return 0;
}

static int parse_string_tag(const char *xml, const char *tag, char *out, size_t out_sz)
{
    char ot[64], ct[64];
    snprintf(ot, sizeof(ot), "<%s>", tag);
    snprintf(ct, sizeof(ct), "</%s>", tag);
    char *s = strstr(xml, ot);
    if (!s) {
        return -1;
    }
    s += strlen(ot);
    char *e = strstr(s, ct);
    if (!e) {
        return -1;
    }
    size_t ln = (size_t)(e - s);
    if (ln >= out_sz) {
        ln = out_sz - 1u;
    }
    strncpy(out, s, ln);
    out[ln] = '\0';
    return 0;
}

void ul_ecat_scan_slaves_with_esi(const char **file_list)
{
    memset(&g_esi_lib, 0, sizeof(g_esi_lib));
    if (file_list) {
        for (int i = 0; file_list[i] != NULL; i++) {
            ul_ecat_esi_parse_result_t tmp;
            memset(&tmp, 0, sizeof(tmp));
            if (ul_ecat_esi_parse_file(file_list[i], &tmp) == 0) {
                merge_esi_res(&tmp);
            }
        }
    }
    if (g_slaves_db.slave_count <= 0) {
        printf("No slaves in database; run scan first.\n");
        return;
    }
    for (int i = 0; i < g_slaves_db.slave_count; i++) {
        ul_ecat_slave_t *s = &g_slaves_db.slaves[i];
        for (int j = 0; j < g_esi_lib.dev_count; j++) {
            ul_ecat_esi_device_t *ed = &g_esi_lib.devs[j];
            if (ed->vendor_id == s->vendor_id && ed->product_code == s->product_code) {
                s->revision_no = ed->revision_no;
                s->serial_no = ed->serial_no;
                if (strlen(ed->device_name) > 0) {
                    strncpy(s->device_name, ed->device_name, sizeof(s->device_name) - 1u);
                }
                break;
            }
        }
    }
    printf("ESI merge done for %d slaves.\n", g_slaves_db.slave_count);
}

/* --- Blocking raw exchange --- */

static ssize_t send_ec_payload(const uint8_t *ec_payload, size_t ec_len)
{
    uint8_t frame[1600];
    ssize_t flen = ul_ecat_build_eth_frame(g_dst_mac, g_src_mac, ec_payload, ec_len, frame, sizeof(frame));
    if (flen < 0) {
        return -1;
    }
    return ul_ecat_transport_send(g_sockfd, frame, (size_t)flen, 0);
}

static int recv_eth_frame(uint8_t *buf, size_t cap, int timeout_ms)
{
    int w = ul_ecat_transport_wait_readable(g_sockfd, timeout_ms);
    if (w <= 0) {
        return -1;
    }
    ssize_t n = ul_ecat_transport_recv(g_sockfd, buf, cap, 0);
    return (int)n;
}

static int exchange_ec_payload(const uint8_t *ec_payload, size_t ec_len,
                               uint8_t *resp_ec, size_t *resp_ec_len, size_t resp_cap,
                               int timeout_ms)
{
    ul_ecat_osal_trx_lock();
    if (send_ec_payload(ec_payload, ec_len) < 0) {
        ul_ecat_osal_trx_unlock();
        return -1;
    }
    uint8_t rx[1600];
    int n = recv_eth_frame(rx, sizeof(rx), timeout_ms);
    if (n < 0) {
        ul_ecat_osal_trx_unlock();
        return -1;
    }
    const uint8_t *pdu = NULL;
    size_t pdu_len = 0;
    if (ul_ecat_parse_eth_frame(rx, (size_t)n, &pdu, &pdu_len) != 0) {
        ul_ecat_osal_trx_unlock();
        return -1;
    }
    if (pdu_len > resp_cap) {
        ul_ecat_osal_trx_unlock();
        return -1;
    }
    memcpy(resp_ec, pdu, pdu_len);
    *resp_ec_len = pdu_len;
    ul_ecat_osal_trx_unlock();
    return 0;
}

static int read_al_status_blocking(uint16_t station, uint16_t *alstat_out, int timeout_ms)
{
    uint8_t raw[2];
    uint16_t wkc = 0;
    if (fprd_blocking(station, ESC_REG_ALSTAT, raw, 2u, &wkc, timeout_ms) != 0) {
        return -1;
    }
    if (wkc < 1u) {
        return -1;
    }
    *alstat_out = r16_le_buf(raw);
    return 0;
}

static int write_al_control_blocking(uint16_t station, uint16_t req_bits, int timeout_ms)
{
    uint8_t buf[2];
    w16_le_buf(buf, req_bits);
    uint16_t wkc = 0;
    if (fpwr_blocking(station, ESC_REG_ALCTL, buf, 2u, &wkc, timeout_ms) != 0) {
        return -1;
    }
    if (wkc < 1u) {
        return -1;
    }
    return 0;
}

int ul_ecat_poll_slave_state(int slave_index, ul_ecat_slave_state_t desired_state, int timeout_ms)
{
    ul_ecat_master_slaves_t *db = ul_ecat_get_master_slaves();
    if (slave_index < 0 || slave_index >= db->slave_count) {
        return -1;
    }
    ul_ecat_slave_t *sl = &db->slaves[slave_index];
    const int step_ms = 20;
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        uint16_t alst = 0;
        if (read_al_status_blocking(sl->station_address, &alst, 200) != 0) {
            elapsed += step_ms;
            ul_ecat_osal_sleep_us((unsigned)(step_ms * 1000));
            continue;
        }
        if (ul_ecat_al_status_error_indicated(alst)) {
            fprintf(stderr, "ul_ecat: AL Status error bit set (0x%04X).\n", alst);
            return -1;
        }
        ul_ecat_slave_state_t cur = (ul_ecat_slave_state_t)ul_ecat_al_status_state(alst);
        if (cur == desired_state) {
            sl->state = desired_state;
            return 0;
        }
        elapsed += step_ms;
        ul_ecat_osal_sleep_us((unsigned)(step_ms * 1000));
    }
    return -1;
}

int ul_ecat_request_slave_state_mailbox(int slave_index, ul_ecat_slave_state_t state)
{
    ul_ecat_master_slaves_t *db = ul_ecat_get_master_slaves();
    if (slave_index < 0 || slave_index >= db->slave_count) {
        return -1;
    }
    ul_ecat_slave_t *sl = &db->slaves[slave_index];
    uint8_t nibble = (uint8_t)((unsigned)state & 0x0Fu);
    if (write_al_control_blocking(sl->station_address, ul_ecat_al_control_word(nibble, 0), 500) != 0) {
        return -1;
    }
    if (ul_ecat_poll_slave_state(slave_index, state, 3000) != 0) {
        return -1;
    }
    if (write_al_control_blocking(sl->station_address, ul_ecat_al_control_word(nibble, 1), 500) != 0) {
        return -1;
    }
    return 0;
}

int ul_ecat_request_slave_state(int slave_index, ul_ecat_slave_state_t state)
{
    return ul_ecat_request_slave_state_mailbox(slave_index, state);
}

/* --- Network scan: APWR station address, then identity reads --- */

static int scan_verbose_env(void)
{
    const char *v = getenv("UL_ECAT_VERBOSE");
    return (v != NULL && v[0] != '\0' && strcmp(v, "0") != 0);
}

static int apwr_blocking(uint16_t adp_pos, uint16_t ado, const void *data, uint16_t len,
                         uint16_t *wkc_out, int timeout_ms)
{
    if (g_scan_trace) {
        printf("[ul_ecat] scan TX: APWR adp=%u ado=0x%04X len=%u\n", (unsigned)adp_pos, (unsigned)ado,
               (unsigned)len);
    }
    uint8_t dg[512];
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_APWR, 0,
                                   adp_pos, ado, len, 0u, 0u, data);
    if (enc < 0) {
        return -1;
    }
    uint8_t resp[512];
    size_t rlen = 0;
    if (exchange_ec_payload(dg, (size_t)enc, resp, &rlen, sizeof(resp), timeout_ms) != 0) {
        if (g_scan_trace) {
            printf("[ul_ecat] scan RX: APWR exchange failed\n");
        }
        return -1;
    }
    uint16_t wkc = 0;
    if (ul_ecat_dgram_parse(resp, rlen, 0, NULL, NULL, NULL, NULL, NULL, NULL, &wkc, NULL, 0) != 0) {
        return -1;
    }
    if (g_scan_trace) {
        printf("[ul_ecat] scan RX: APWR WKC=%u\n", (unsigned)wkc);
    }
    if (wkc_out) {
        *wkc_out = wkc;
    }
    return 0;
}

static int fprd_blocking(uint16_t station, uint16_t ado, void *out, uint16_t len,
                         uint16_t *wkc_out, int timeout_ms)
{
    if (g_scan_trace) {
        printf("[ul_ecat] scan TX: FPRD adp=0x%04X ado=0x%04X len=%u\n", (unsigned)station,
               (unsigned)ado, (unsigned)len);
    }
    uint8_t dg[512];
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0,
                                   station, ado, len, 0u, 0u, NULL);
    if (enc < 0) {
        return -1;
    }
    uint8_t resp[512];
    size_t rlen = 0;
    if (exchange_ec_payload(dg, (size_t)enc, resp, &rlen, sizeof(resp), timeout_ms) != 0) {
        if (g_scan_trace) {
            printf("[ul_ecat] scan RX: FPRD ado=0x%04X exchange failed\n", (unsigned)ado);
        }
        return -1;
    }
    uint16_t wkc = 0;
    if (ul_ecat_dgram_parse(resp, rlen, 0, NULL, NULL, NULL, NULL, NULL, NULL, &wkc, out, len) != 0) {
        return -1;
    }
    if (g_scan_trace) {
        printf("[ul_ecat] scan RX: FPRD ado=0x%04X WKC=%u\n", (unsigned)ado, (unsigned)wkc);
    }
    if (wkc_out) {
        *wkc_out = wkc;
    }
    return 0;
}

static int fpwr_blocking(uint16_t adp, uint16_t ado, const void *data, uint16_t len,
                         uint16_t *wkc_out, int timeout_ms)
{
    uint8_t dg[512];
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPWR, 0,
                                   adp, ado, len, 0u, 0u, data);
    if (enc < 0) {
        return -1;
    }
    uint8_t resp[512];
    size_t rlen = 0;
    if (exchange_ec_payload(dg, (size_t)enc, resp, &rlen, sizeof(resp), timeout_ms) != 0) {
        return -1;
    }
    uint16_t wkc = 0;
    if (ul_ecat_dgram_parse(resp, rlen, 0, NULL, NULL, NULL, NULL, NULL, NULL, &wkc, NULL, 0) != 0) {
        return -1;
    }
    if (wkc_out) {
        *wkc_out = wkc;
    }
    return 0;
}

int ul_ecat_fprd_sync(uint16_t adp, uint16_t ado, uint16_t len, uint8_t *out, size_t out_cap, int timeout_ms)
{
    if (!out || len == 0u || out_cap < (size_t)len || g_sockfd < 0) {
        return -1;
    }
    if (len > 512u) {
        return -1;
    }
    uint16_t wkc = 0;
    if (fprd_blocking(adp, ado, out, len, &wkc, timeout_ms) != 0) {
        return -1;
    }
    if (wkc < 1u) {
        return -1;
    }
    return (int)len;
}

int ul_ecat_fpwr_sync(uint16_t adp, uint16_t ado, const void *data, uint16_t len, int timeout_ms)
{
    if (!data || len == 0u || g_sockfd < 0) {
        return -1;
    }
    if (len > 512u) {
        return -1;
    }
    uint16_t wkc = 0;
    if (fpwr_blocking(adp, ado, data, len, &wkc, timeout_ms) != 0) {
        return -1;
    }
    if (wkc < 1u) {
        return -1;
    }
    return 0;
}

int ul_ecat_scan_network(void)
{
    g_scan_trace = scan_verbose_env();
    if (g_scan_trace) {
        printf("[ul_ecat] scan: start (verbose trace enabled via UL_ECAT_VERBOSE)\n");
    }

    memset(&g_slaves_db, 0, sizeof(g_slaves_db));
    g_slaves_db.slave_count = 0;

    for (int pos = 0; pos < UL_ECAT_MAX_SLAVES; pos++) {
        uint16_t station = (uint16_t)(0x1000 + pos);
        uint8_t stadr[2];
        w16_le_buf(stadr, station);
        uint16_t wkc = 0;
        if (g_scan_trace) {
            printf("[ul_ecat] scan: logical position %d -> assign station 0x%04X\n", pos, (unsigned)station);
        }
        if (apwr_blocking((uint16_t)pos, ESC_REG_STADR, stadr, 2u, &wkc, 200) != 0) {
            break;
        }
        if (wkc < 1u) {
            if (g_scan_trace) {
                printf("[ul_ecat] scan: no slave at position %d (WKC=0), end of ring\n", pos);
            }
            break;
        }
        ul_ecat_slave_t *s = &g_slaves_db.slaves[g_slaves_db.slave_count];
        s->station_address = station;
        s->state = UL_ECAT_SLAVE_STATE_INIT;
        uint32_t vid = 0, pid = 0, rev = 0, ser = 0;
        uint8_t tmp[4];
        if (fprd_blocking(station, UL_ECAT_ESC_REG_VENDOR, tmp, 4u, &wkc, 200) == 0 && wkc >= 1u) {
            vid = (uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
        }
        if (fprd_blocking(station, UL_ECAT_ESC_REG_PRODUCT, tmp, 4u, &wkc, 200) == 0 && wkc >= 1u) {
            pid = (uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
        }
        if (fprd_blocking(station, UL_ECAT_ESC_REG_REV, tmp, 4u, &wkc, 200) == 0 && wkc >= 1u) {
            rev = (uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
        }
        if (fprd_blocking(station, UL_ECAT_ESC_REG_SERIAL, tmp, 4u, &wkc, 200) == 0 && wkc >= 1u) {
            ser = (uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
        }
        s->vendor_id = vid;
        s->product_code = pid;
        s->revision_no = rev;
        s->serial_no = ser;
        g_slaves_db.slave_count++;
    }

    printf("Scan done. Found %d slaves.\n", g_slaves_db.slave_count);
    for (int i = 0; i < g_slaves_db.slave_count; i++) {
        ul_ecat_slave_t *sl = &g_slaves_db.slaves[i];
        printf("Slave[%d]: station=0x%04X vend=0x%08X prod=0x%08X rev=0x%08X ser=0x%08X\n",
               i, sl->station_address, sl->vendor_id, sl->product_code,
               sl->revision_no, sl->serial_no);
    }
    return 0;
}

/* --- DC --- */

static void dc_init(void)
{
    g_dc_state = 1;
    g_dc_offset = 0;
    g_dc_timeout = 0;
    g_dc_error_code = 0;
    printf("DC: transition to INIT\n");
    ul_ecat_dc_event_info_t ev;
    ev.event = UL_ECAT_DC_EVENT_INIT;
    ev.offset_ns = 0;
    ev.error_code = 0;
    ul_ecat_eventloop_post_dc_event(&ev);
}

static void dc_cycle(void)
{
    if (g_dc_state == 0) {
        return;
    }
    if (g_dc_state == 3) {
        return;
    }
    if (g_dc_state == 1) {
        g_dc_state = 2;
        printf("DC: transition to OP (logical)\n");
    }
    if (g_dc_state == 2) {
        ul_ecat_queue_fprd(0x0000u, ESC_REG_DCSYS0, 8u);
        g_dc_timeout++;
        if (g_dc_timeout > 50) {
            g_dc_error_code = 2;
            g_dc_state = 3;
            ul_ecat_dc_event_info_t e;
            e.event = UL_ECAT_DC_EVENT_ERROR;
            e.offset_ns = g_dc_offset;
            e.error_code = g_dc_error_code;
            ul_ecat_eventloop_post_dc_event(&e);
        }
    }
}

static void handle_dc_response(uint64_t slave_time_ns)
{
    uint64_t master = ul_ecat_osal_monotonic_ns();
    int64_t off = (int64_t)slave_time_ns - (int64_t)master;
    g_dc_offset = off;
    g_dc_timeout = 0;

    if (llabs(off) > g_dc_threshold_ns) {
        g_dc_error_code = 1;
        g_dc_state = 3;
        ul_ecat_dc_event_info_t e;
        e.event = UL_ECAT_DC_EVENT_ERROR;
        e.offset_ns = off;
        e.error_code = g_dc_error_code;
        ul_ecat_eventloop_post_dc_event(&e);
    } else {
        int32_t corr = (int32_t)(-off);
        ul_ecat_queue_fpwr(0x0000u, ESC_REG_DCSYSOFS, &corr, 4u);
        ul_ecat_dc_event_info_t e;
        e.event = UL_ECAT_DC_EVENT_SYNC;
        e.offset_ns = off;
        e.error_code = 0;
        ul_ecat_eventloop_post_dc_event(&e);
    }
}

/* --- Queue (fixed buffers, mutex) --- */

static int q_push_encoded(const uint8_t *wire, uint16_t wire_len)
{
    ul_ecat_osal_q_lock();
    if (g_q_count >= Q_MAX) {
        ul_ecat_osal_q_unlock();
        fprintf(stderr, "Datagram queue full.\n");
        return -1;
    }
    if (wire_len > DGRAM_BUF) {
        ul_ecat_osal_q_unlock();
        return -1;
    }
    memcpy(g_q[g_q_count].data, wire, wire_len);
    g_q[g_q_count].wire_len = wire_len;
    g_q_count++;
    ul_ecat_osal_q_unlock();
    return 0;
}

void ul_ecat_queue_fprd(uint16_t adp, uint16_t ado, uint16_t length)
{
    uint8_t tmp[DGRAM_BUF];
    int n = ul_ecat_dgram_encode(tmp, sizeof(tmp), UL_ECAT_CMD_FPRD, g_dgram_index_seq++,
                                 adp, ado, length, 0u, 0u, NULL);
    if (n < 0) {
        return;
    }
    if (q_push_encoded(tmp, (uint16_t)n) == 0) {
        printf("Queue FPRD: adp=0x%04X ado=0x%04X len=%u\n", adp, ado, (unsigned)length);
    }
}

void ul_ecat_queue_fpwr(uint16_t adp, uint16_t ado, const void *data, uint16_t length)
{
    uint8_t tmp[DGRAM_BUF];
    int n = ul_ecat_dgram_encode(tmp, sizeof(tmp), UL_ECAT_CMD_FPWR, g_dgram_index_seq++,
                                 adp, ado, length, 0u, 0u, data);
    if (n < 0) {
        return;
    }
    if (q_push_encoded(tmp, (uint16_t)n) == 0) {
        printf("Queue FPWR: adp=0x%04X ado=0x%04X len=%u\n", adp, ado, (unsigned)length);
    }
}

static void process_rx_datagrams(const uint8_t *pdu, size_t pdu_len)
{
    int nd = ul_ecat_pdu_count_datagrams(pdu, pdu_len);
    if (nd < 0) {
        return;
    }
    for (int i = 0; i < nd; i++) {
        uint8_t cmd = 0;
        uint16_t ado = 0;
        uint16_t adp = 0;
        uint16_t dlen = 0;
        uint16_t wkc = 0;
        uint8_t datab[32];
        if (ul_ecat_dgram_parse(pdu, pdu_len, (unsigned)i, &cmd, NULL, &adp, &ado, &dlen, NULL, &wkc,
                                datab, sizeof(datab)) != 0) {
            continue;
        }
        if (wkc == 0u) {
            fprintf(stderr, "ul_ecat: RX datagram[%d] has WKC=0 (adp=0x%04X ado=0x%04X).\n", i, adp, ado);
        }
        if (cmd == UL_ECAT_CMD_FPRD && ado == ESC_REG_DCSYS0 && dlen >= 8u && wkc >= 1u) {
            uint64_t st = 0;
            memcpy(&st, datab, 8);
            handle_dc_response(st);
        }
    }
}

void ul_ecat_send_batched_frames(void)
{
    ul_ecat_osal_trx_lock();
    ul_ecat_osal_q_lock();
    if (g_sockfd < 0) {
        ul_ecat_osal_q_unlock();
        ul_ecat_osal_trx_unlock();
        return;
    }
    size_t off = 0;
    uint8_t ec_buf[UL_ECAT_MAX_EC_PAYLOAD];
    int i = 0;
    while (i < g_q_count) {
        off = 0;
        while (i < g_q_count) {
            if (off + g_q[i].wire_len > sizeof(ec_buf)) {
                break;
            }
            memcpy(ec_buf + off, g_q[i].data, g_q[i].wire_len);
            off += g_q[i].wire_len;
            i++;
        }
        uint8_t frame[1600];
        ssize_t flen = ul_ecat_build_eth_frame(g_dst_mac, g_src_mac, ec_buf, off, frame, sizeof(frame));
        if (flen < 0) {
            break;
        }
        if (ul_ecat_transport_send(g_sockfd, frame, (size_t)flen, 0) < 0) {
            perror("send EtherCAT frame");
        }
    }
    g_q_count = 0;
    ul_ecat_osal_q_unlock();
    ul_ecat_osal_trx_unlock();
}

void ul_ecat_receive_frames_nonblock(void)
{
    ul_ecat_osal_trx_lock();
    if (g_sockfd < 0) {
        ul_ecat_osal_trx_unlock();
        return;
    }
    int ret = ul_ecat_transport_wait_readable(g_sockfd, 0);
    if (ret <= 0) {
        ul_ecat_osal_trx_unlock();
        return;
    }
    while (1) {
        uint8_t rx[1600];
        ssize_t r = ul_ecat_transport_recv(g_sockfd, rx, sizeof(rx), MSG_DONTWAIT);
        if (r < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
            }
            break;
        }
        const uint8_t *pdu = NULL;
        size_t pdu_len = 0;
        if (ul_ecat_parse_eth_frame(rx, (size_t)r, &pdu, &pdu_len) == 0) {
            process_rx_datagrams(pdu, pdu_len);
            struct ul_ecat_frame fr;
            memset(&fr, 0, sizeof(fr));
            if ((size_t)r <= sizeof(fr.dst_mac) + sizeof(fr.src_mac) + sizeof(fr.ethertype) + sizeof(fr.payload)) {
                memcpy(fr.dst_mac, rx, 6);
                memcpy(fr.src_mac, rx + 6, 6);
                fr.ethertype = (unsigned short)(((uint16_t)rx[12] << 8) | (uint16_t)rx[13]);
                size_t pay = (size_t)r - 14u;
                if (pay > sizeof(fr.payload)) {
                    pay = sizeof(fr.payload);
                }
                memcpy(fr.payload, rx + 14, pay);
                ul_ecat_eventloop_post_frame_event(&fr, r);
            }
        }
    }
    ul_ecat_osal_trx_unlock();
}

/* --- RT / lifecycle --- */

static void *periodic_thread_func(void *arg)
{
    (void)arg;
    if (g_dc_state == 1) {
        dc_init();
    }
    uint64_t next_wake = ul_ecat_osal_monotonic_ns();
    while (g_running) {
        dc_cycle();
        ul_ecat_send_batched_frames();
        ul_ecat_receive_frames_nonblock();
        ul_ecat_osal_sleep_until_ns(&next_wake, CYCLE_TIME_NS);
    }
    printf("periodic_thread_func: exit.\n");
    return NULL;
}

int ul_ecat_master_init(const ul_ecat_master_settings_t *cfg)
{
    if (!cfg) {
        fprintf(stderr, "No config.\n");
        return -1;
    }
    ul_ecat_osal_init();
    int s = ul_ecat_transport_open(cfg->iface_name);
    if (s < 0) {
        return -1;
    }
    g_sockfd = s;

    ul_ecat_osal_realtime_hint(cfg->rt_priority);

    memcpy(g_dst_mac, cfg->dst_mac, 6);
    memcpy(g_src_mac, cfg->src_mac, 6);

    if (cfg->dc_enable) {
        g_dc_state = 1;
    } else {
        g_dc_state = 0;
    }
    g_running = 1;
    g_dgram_index_seq = 0;

    if (ul_ecat_osal_worker_start(periodic_thread_func, NULL, cfg->rt_priority) != 0) {
        ul_ecat_transport_close(g_sockfd);
        g_sockfd = -1;
        return -1;
    }
    return 0;
}

int ul_ecat_master_shutdown(void)
{
    if (g_sockfd < 0) {
        fprintf(stderr, "Master not running.\n");
        return -1;
    }
    g_running = 0;
    ul_ecat_osal_worker_join();
    ul_ecat_transport_close(g_sockfd);
    g_sockfd = -1;
    ul_ecat_osal_shutdown();
    printf("Master shutdown.\n");
    return 0;
}

/* --- Event loop (unchanged structure, English-only user-facing strings minimal) --- */

typedef enum {
    EV_DC = 1,
    EV_FRAME
} ecat_ev_type_t;

typedef struct {
    ecat_ev_type_t type;
    union {
        ul_ecat_dc_event_info_t dc;
        struct {
            struct ul_ecat_frame frame;
            ssize_t frame_len;
        } fr_evt;
    };
} ecat_event_t;

#define MAX_EVENTS 64
static ecat_event_t g_evt_queue[MAX_EVENTS];
static int g_evt_count = 0;
static volatile int g_evt_stop = 0;
static ul_ecat_dc_callback_t g_cb_dc = NULL;
static ul_ecat_frame_callback_t g_cb_fr = NULL;

void ul_ecat_register_dc_callback(ul_ecat_dc_callback_t cb)
{
    ul_ecat_osal_evt_lock();
    g_cb_dc = cb;
    ul_ecat_osal_evt_unlock();
}

void ul_ecat_register_frame_callback(ul_ecat_frame_callback_t cb)
{
    ul_ecat_osal_evt_lock();
    g_cb_fr = cb;
    ul_ecat_osal_evt_unlock();
}

static void post_evt(const ecat_event_t *evt)
{
    ul_ecat_osal_evt_lock();
    if (g_evt_count < MAX_EVENTS) {
        g_evt_queue[g_evt_count] = *evt;
        g_evt_count++;
        ul_ecat_osal_evt_signal();
    } else {
        fprintf(stderr, "Event queue full.\n");
    }
    ul_ecat_osal_evt_unlock();
}

static void ul_ecat_eventloop_post_dc_event(const ul_ecat_dc_event_info_t *dcinfo)
{
    ecat_event_t e;
    e.type = EV_DC;
    e.dc = *dcinfo;
    post_evt(&e);
}

static void ul_ecat_eventloop_post_frame_event(const struct ul_ecat_frame *fr, ssize_t flen)
{
    ecat_event_t e;
    e.type = EV_FRAME;
    memcpy(&e.fr_evt.frame, fr, sizeof(e.fr_evt.frame));
    e.fr_evt.frame_len = flen;
    post_evt(&e);
}

void ul_ecat_eventloop_run(void)
{
    ul_ecat_osal_evt_lock();
    g_evt_stop = 0;
    ul_ecat_osal_evt_unlock();
    for (;;) {
        ul_ecat_osal_evt_lock();
        while (g_evt_count == 0 && !g_evt_stop) {
            ul_ecat_osal_evt_wait();
        }
        if (g_evt_stop) {
            ul_ecat_osal_evt_unlock();
            break;
        }
        ecat_event_t e = g_evt_queue[0];
        for (int i = 1; i < g_evt_count; i++) {
            g_evt_queue[i - 1] = g_evt_queue[i];
        }
        g_evt_count--;
        ul_ecat_osal_evt_unlock();
        if (e.type == EV_DC && g_cb_dc) {
            g_cb_dc(&e.dc);
        } else if (e.type == EV_FRAME && g_cb_fr) {
            g_cb_fr(&e.fr_evt.frame, e.fr_evt.frame_len);
        }
    }
}

void ul_ecat_eventloop_stop(void)
{
    ul_ecat_osal_evt_lock();
    g_evt_stop = 1;
    ul_ecat_osal_evt_signal();
    ul_ecat_osal_evt_unlock();
}

/* --- CLI --- */

int ul_ecat_app_execute(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: %s <iface> <command> [args...]\n", argv[0]);
        printf("Commands:\n");
        printf("  scan\n");
        printf("  scan-esi <file1.xml> ... null\n");
        printf("  slave-state <idx> <state_dec>\n");
        printf("  read <adp_hex> <ado_hex> <len_dec>\n");
        printf("  write <adp_hex> <ado_hex> <val_hex> [size=4]\n");
        printf("  stop\n");
        return 1;
    }
    ul_ecat_master_settings_t cfg = {
        .iface_name = argv[1],
        .dst_mac = {0},
        .src_mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01},
        .rt_priority = 50,
        .dc_enable = 0,
        .dc_sync0_cycle = 1000000
    };
    ul_ecat_mac_broadcast(cfg.dst_mac);
    if (ul_ecat_master_init(&cfg) != 0) {
        fprintf(stderr, "Failed to init.\n");
        return 1;
    }
    const char *cmd = argv[2];
    if (strcmp(cmd, "scan") == 0) {
        ul_ecat_scan_network();
    } else if (strcmp(cmd, "scan-esi") == 0) {
        if (argc < 4) {
            printf("Usage: ... scan-esi <file1.xml> ... null\n");
            ul_ecat_master_shutdown();
            return 1;
        }
        static const char *files[64];
        int fc = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "null") == 0) {
                break;
            }
            files[fc] = argv[i];
            fc++;
            if (fc >= 63) {
                break;
            }
        }
        files[fc] = NULL;
        ul_ecat_scan_slaves_with_esi(files);
    } else if (strcmp(cmd, "slave-state") == 0) {
        if (argc < 5) {
            printf("Not enough args.\n");
            ul_ecat_master_shutdown();
            return 1;
        }
        int idx = atoi(argv[3]);
        int st = atoi(argv[4]);
        ul_ecat_request_slave_state(idx, (ul_ecat_slave_state_t)st);
    } else if (strcmp(cmd, "read") == 0) {
        if (argc < 6) {
            printf("read <adp_hex> <ado_hex> <len_dec>\n");
            ul_ecat_master_shutdown();
            return 1;
        }
        uint16_t adp = (uint16_t)strtol(argv[3], NULL, 16);
        uint16_t ado = (uint16_t)strtol(argv[4], NULL, 16);
        uint16_t ln = (uint16_t)strtol(argv[5], NULL, 0);
        uint8_t buf[512];
        if (ln == 0u || ln > sizeof(buf)) {
            printf("len must be 1..%zu\n", sizeof(buf));
            ul_ecat_master_shutdown();
            return 1;
        }
        int n = ul_ecat_fprd_sync(adp, ado, ln, buf, sizeof(buf), 2000);
        if (n < 0) {
            fprintf(stderr, "FPRD failed (timeout or WKC=0).\n");
        } else {
            for (int i = 0; i < n; i++) {
                printf("%02X ", buf[i]);
            }
            printf("\n");
        }
    } else if (strcmp(cmd, "write") == 0) {
        if (argc < 6) {
            printf("write <adp_hex> <ado_hex> <val_hex> [size=4]\n");
            ul_ecat_master_shutdown();
            return 1;
        }
        uint16_t adp = (uint16_t)strtol(argv[3], NULL, 16);
        uint16_t ado = (uint16_t)strtol(argv[4], NULL, 16);
        uint32_t val = (uint32_t)strtol(argv[5], NULL, 16);
        uint16_t sz = 4;
        if (argc >= 7) {
            sz = (uint16_t)strtol(argv[6], NULL, 0);
        }
        if (sz == 0u || sz > 4u) {
            printf("size must be 1..4\n");
            ul_ecat_master_shutdown();
            return 1;
        }
        if (ul_ecat_fpwr_sync(adp, ado, &val, sz, 2000) != 0) {
            fprintf(stderr, "FPWR failed (timeout or WKC=0).\n");
        }
    } else if (strcmp(cmd, "stop") == 0) {
        ul_ecat_master_shutdown();
        return 0;
    } else {
        printf("Unknown command: %s\n", cmd);
    }
    ul_ecat_master_shutdown();
    return 0;
}
