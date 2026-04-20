/**
 * @file test_coe.cpp
 * @brief CoE/SDO server expedited transfer tests via the mailbox path.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_frame.h"
#include "ul_ecat_slave.h"
#include "ul_ecat_slave_controller.h"
#include "ul_ecat_slave_od.h"
#include "ul_ecat_slave_tables.h"
}

static const ul_ecat_slave_identity_t TEST_ID = {0x02u, 0x1C213052u, 1u, 1u};
static const uint8_t TEST_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static constexpr uint16_t SM0_START = 0x0D00u;
static constexpr uint16_t SM_LEN    = 128u;
static constexpr uint16_t SM1_START = 0x0D80u;

/* Bits we use across tests. */
#define COE_SDO_REQ      0x02u
#define COE_SDO_RESP     0x03u
#define CCS_UPLOAD_INIT  (2u << 5)
#define CCS_DOWNLOAD_INIT (1u << 5)
#define SCS_UPLOAD_RESP  (2u << 5)
#define SCS_DOWNLOAD_RESP (3u << 5)
#define SDO_CMD_ABORT    (4u << 5)
#define SDO_BIT_EXP      0x02u
#define SDO_BIT_SIZE_IND 0x01u

class Coe : public ::testing::Test {
protected:
    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};

    void SetUp() override {
        ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave,
                    UL_ECAT_SLAVE_BACKEND_SOFTWARE_ETHERNET, TEST_MAC, &TEST_ID), 0);
        assign_station(0x1000u);
        configure_mailbox_sm();
    }

    void assign_station(uint16_t station) {
        uint8_t buf[2] = {(uint8_t)(station & 0xFF), (uint8_t)(station >> 8)};
        fpwr(station, UL_ECAT_ESC_REG_STADR, buf, 2, true);
    }

    void configure_mailbox_sm() {
        uint8_t sm0[8] = {(uint8_t)(SM0_START & 0xFF), (uint8_t)(SM0_START >> 8),
                          (uint8_t)(SM_LEN & 0xFF), (uint8_t)(SM_LEN >> 8),
                          0x26, 0x00, 0x01, 0x00};
        fpwr(0x1000u, UL_ECAT_ESC_REG_SM0, sm0, 8);
        uint8_t sm1[8] = {(uint8_t)(SM1_START & 0xFF), (uint8_t)(SM1_START >> 8),
                          (uint8_t)(SM_LEN & 0xFF), (uint8_t)(SM_LEN >> 8),
                          0x22, 0x00, 0x01, 0x00};
        fpwr(0x1000u, UL_ECAT_ESC_REG_SM1, sm1, 8);
    }

    uint16_t fpwr(uint16_t station, uint16_t ado, const uint8_t *data, uint16_t len, bool use_apwr = false) {
        uint8_t dg[512], pdu_out[512];
        size_t pdu_out_len = 0;
        uint8_t cmd = use_apwr ? UL_ECAT_CMD_APWR : UL_ECAT_CMD_FPWR;
        uint16_t adp = use_apwr ? 0u : station;
        int enc = ul_ecat_dgram_encode(dg, sizeof(dg), cmd, 0, adp, ado, len, 0, 0, data);
        EXPECT_GT(enc, 0);
        EXPECT_EQ(ul_ecat_slave_process_pdu(&slave, dg, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
        uint16_t wkc = 0;
        ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &wkc, nullptr, 0);
        return wkc;
    }

    /* Build a CoE/SDO mailbox frame of fixed SM_LEN bytes. */
    std::vector<uint8_t> build_sdo_frame(uint8_t cmd, uint16_t index, uint8_t subindex,
                                         uint32_t data) {
        std::vector<uint8_t> f(SM_LEN, 0);
        /* mbx hdr: length=COE_HDR+SDO_HDR=10, address=0x1000, channel=0, type=CoE */
        f[0] = 10; f[1] = 0;
        f[2] = 0x00; f[3] = 0x10;
        f[4] = 0x00;
        f[5] = UL_ECAT_MBX_TYPE_COE;
        /* coe hdr: number=0, service=SDO Req(0x02) in high nibble of byte 1 */
        f[6] = 0x00;
        f[7] = (uint8_t)(COE_SDO_REQ << 4);
        /* sdo hdr */
        f[8] = cmd;
        f[9]  = (uint8_t)(index & 0xFF);
        f[10] = (uint8_t)(index >> 8);
        f[11] = subindex;
        f[12] = (uint8_t)(data & 0xFF);
        f[13] = (uint8_t)((data >> 8) & 0xFF);
        f[14] = (uint8_t)((data >> 16) & 0xFF);
        f[15] = (uint8_t)((data >> 24) & 0xFF);
        return f;
    }

    /* Send a CoE request via the slave_controller path and read the SM1 reply. */
    std::vector<uint8_t> sdo_round_trip(const std::vector<uint8_t> &request) {
        uint8_t rx[1600], tx[1600];
        uint8_t dg[512];
        int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPWR, 0, 0x1000u, SM0_START,
                                       (uint16_t)request.size(), 0, 0, request.data());
        EXPECT_GT(enc, 0);
        uint8_t master_mac[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        ssize_t n = ul_ecat_build_eth_frame(TEST_MAC, master_mac, dg, (size_t)enc, rx, sizeof(rx));
        EXPECT_GT(n, 0);
        size_t tx_len = 0;
        EXPECT_EQ(ul_ecat_slave_controller_process_ethernet(&ctrl, rx, (size_t)n,
                                                            tx, sizeof(tx), &tx_len), 0);

        /* Master FPRD on SM1 to retrieve the reply. */
        uint8_t pdu_out[512];
        size_t pdu_out_len = 0;
        int enc2 = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000u, SM1_START,
                                        SM_LEN, 0, 0, nullptr);
        EXPECT_GT(enc2, 0);
        EXPECT_EQ(ul_ecat_slave_process_pdu(&slave, dg, (size_t)enc2, pdu_out, sizeof(pdu_out),
                                            &pdu_out_len), 0);
        std::vector<uint8_t> reply(SM_LEN);
        uint16_t wkc = 0;
        EXPECT_EQ(ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr, &wkc, reply.data(), reply.size()), 0);
        EXPECT_GE(wkc, 1u);
        return reply;
    }
};

TEST_F(Coe, UploadVendorIdReturnsExpedited4Bytes) {
    auto req = build_sdo_frame(CCS_UPLOAD_INIT, 0x1018, 0x01, 0u);
    auto rep = sdo_round_trip(req);
    /* CoE service in reply byte 7 high nibble must be SDO Resp. */
    EXPECT_EQ(rep[7] >> 4, COE_SDO_RESP);
    /* SDO scs == upload init resp, expedited, size indicated, n=0 (4-byte payload). */
    uint8_t scs = rep[8] & 0xE0u;
    EXPECT_EQ(scs, SCS_UPLOAD_RESP);
    EXPECT_NE(rep[8] & SDO_BIT_EXP, 0u);
    EXPECT_NE(rep[8] & SDO_BIT_SIZE_IND, 0u);
    /* Index/subindex echoed. */
    EXPECT_EQ((uint16_t)rep[9] | ((uint16_t)rep[10] << 8), 0x1018u);
    EXPECT_EQ(rep[11], 0x01u);
    /* Vendor matches generated identity. */
    uint32_t v = (uint32_t)rep[12] | ((uint32_t)rep[13] << 8) |
                 ((uint32_t)rep[14] << 16) | ((uint32_t)rep[15] << 24);
    EXPECT_EQ(v, ul_ecat_generated_slave_identity.vendor_id);
}

TEST_F(Coe, UploadIdentitySubindex0IsCount4) {
    auto req = build_sdo_frame(CCS_UPLOAD_INIT, 0x1018, 0x00, 0u);
    auto rep = sdo_round_trip(req);
    EXPECT_EQ(rep[8] & 0xE0u, SCS_UPLOAD_RESP);
    /* Length is 1 byte -> n field == 3. */
    EXPECT_EQ((rep[8] >> 2) & 0x03u, 3u);
    EXPECT_EQ(rep[12], 4u);
}

TEST_F(Coe, UploadUnknownObjectAborts0x06020000) {
    auto req = build_sdo_frame(CCS_UPLOAD_INIT, 0x9999, 0x00, 0u);
    auto rep = sdo_round_trip(req);
    EXPECT_EQ(rep[8] & 0xE0u, SDO_CMD_ABORT);
    uint32_t code = (uint32_t)rep[12] | ((uint32_t)rep[13] << 8) |
                    ((uint32_t)rep[14] << 16) | ((uint32_t)rep[15] << 24);
    EXPECT_EQ(code, 0x06020000u);
}

TEST_F(Coe, UploadKnownIndexBadSubindexAborts0x06090011) {
    auto req = build_sdo_frame(CCS_UPLOAD_INIT, 0x1018, 0xFF, 0u);
    auto rep = sdo_round_trip(req);
    EXPECT_EQ(rep[8] & 0xE0u, SDO_CMD_ABORT);
    uint32_t code = (uint32_t)rep[12] | ((uint32_t)rep[13] << 8) |
                    ((uint32_t)rep[14] << 16) | ((uint32_t)rep[15] << 24);
    EXPECT_EQ(code, 0x06090011u);
}

TEST_F(Coe, DownloadToReadOnlyAborts0x06010002) {
    /* expedited download, size=4, value=0xDEADBEEF -> 0x1018:01 (RO) */
    uint8_t cmd = (uint8_t)(CCS_DOWNLOAD_INIT | SDO_BIT_EXP | SDO_BIT_SIZE_IND);  /* n=0 -> 4 bytes */
    auto req = build_sdo_frame(cmd, 0x1018, 0x01, 0xDEADBEEFu);
    auto rep = sdo_round_trip(req);
    EXPECT_EQ(rep[8] & 0xE0u, SDO_CMD_ABORT);
    uint32_t code = (uint32_t)rep[12] | ((uint32_t)rep[13] << 8) |
                    ((uint32_t)rep[14] << 16) | ((uint32_t)rep[15] << 24);
    EXPECT_EQ(code, 0x06010002u);
}

TEST_F(Coe, DownloadThenUploadRoundtripOnRwEntry) {
    /* Install a custom OD with one RW U32 at 0x2000:0 backed by local storage. */
    static uint32_t backing = 0u;
    static const ul_ecat_od_entry_t ENTRIES[] = {
        {0x2000, 0x00, UL_ECAT_OD_TYPE_U32, UL_ECAT_OD_FLAG_RW, 4, &backing},
    };
    static const ul_ecat_od_table_t TBL = {ENTRIES, 1};
    ul_ecat_od_set_table(&TBL);

    uint8_t cmd_dl = (uint8_t)(CCS_DOWNLOAD_INIT | SDO_BIT_EXP | SDO_BIT_SIZE_IND);
    auto rep_dl = sdo_round_trip(build_sdo_frame(cmd_dl, 0x2000, 0x00, 0xCAFEBABEu));
    EXPECT_EQ(rep_dl[8] & 0xE0u, SCS_DOWNLOAD_RESP);
    EXPECT_EQ(backing, 0xCAFEBABEu);

    auto rep_ul = sdo_round_trip(build_sdo_frame(CCS_UPLOAD_INIT, 0x2000, 0x00, 0u));
    EXPECT_EQ(rep_ul[8] & 0xE0u, SCS_UPLOAD_RESP);
    uint32_t v = (uint32_t)rep_ul[12] | ((uint32_t)rep_ul[13] << 8) |
                 ((uint32_t)rep_ul[14] << 16) | ((uint32_t)rep_ul[15] << 24);
    EXPECT_EQ(v, 0xCAFEBABEu);

    /* Restore the generated table so other tests aren't affected. */
    ul_ecat_od_set_table(&ul_ecat_generated_od_table);
}
