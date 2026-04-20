/**
 * @file test_mailbox.cpp
 * @brief Slave mailbox transport (SM0/SM1 handshake) tests.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_frame.h"
#include "ul_ecat_slave.h"
#include "ul_ecat_slave_controller.h"
}

static const ul_ecat_slave_identity_t TEST_ID = {0x02u, 0x1C213052u, 1u, 1u};
static const uint8_t TEST_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

/* Mailbox buffers live inside the 4 KiB ESC mirror (software backend). */
static constexpr uint16_t SM0_START = 0x0D00u;
static constexpr uint16_t SM_LEN    = 128u;
static constexpr uint16_t SM1_START = 0x0D80u;

struct MailboxCapture {
    std::vector<uint8_t> last_frame;
    int calls = 0;
};

static void capture_handler(const uint8_t *frame, size_t len, void *ctx)
{
    auto *c = static_cast<MailboxCapture *>(ctx);
    c->last_frame.assign(frame, frame + len);
    c->calls++;
}

class Mailbox : public ::testing::Test {
protected:
    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    MailboxCapture capture{};

    void SetUp() override {
        ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave,
                    UL_ECAT_SLAVE_BACKEND_SOFTWARE_ETHERNET, TEST_MAC, &TEST_ID), 0);
        ul_ecat_slave_controller_set_mailbox_handler(&ctrl, capture_handler, &capture);
        assign_station(0x1000u);
        configure_mailbox_sm();
    }

    void assign_station(uint16_t station) {
        uint8_t buf[2] = {(uint8_t)(station & 0xFF), (uint8_t)(station >> 8)};
        fpwr(station, UL_ECAT_ESC_REG_STADR, buf, 2, true);
    }

    void configure_mailbox_sm() {
        uint8_t sm0[8] = {
            (uint8_t)(SM0_START & 0xFF), (uint8_t)(SM0_START >> 8),
            (uint8_t)(SM_LEN & 0xFF), (uint8_t)(SM_LEN >> 8),
            0x26, 0x00, 0x01, 0x00,  /* mailbox+write, activated */
        };
        fpwr(0x1000u, UL_ECAT_ESC_REG_SM0, sm0, 8);
        uint8_t sm1[8] = {
            (uint8_t)(SM1_START & 0xFF), (uint8_t)(SM1_START >> 8),
            (uint8_t)(SM_LEN & 0xFF), (uint8_t)(SM_LEN >> 8),
            0x22, 0x00, 0x01, 0x00,  /* mailbox+read, activated */
        };
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

    /* Build an Ethernet frame holding one FPWR datagram (for controller path). */
    size_t build_eth_fpwr(uint8_t *tx, size_t tx_cap, uint16_t ado, const uint8_t *data, uint16_t len) {
        uint8_t dg[512];
        int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPWR, 0, 0x1000u, ado, len, 0, 0, data);
        if (enc <= 0) return 0;
        uint8_t master_mac[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        ssize_t n = ul_ecat_build_eth_frame(TEST_MAC, master_mac, dg, (size_t)enc, tx, tx_cap);
        return n > 0 ? (size_t)n : 0;
    }

    uint8_t sm_status(unsigned sm_index) {
        uint16_t base = (uint16_t)(UL_ECAT_ESC_REG_SM0 + sm_index * UL_ECAT_ESC_SM_SIZE);
        return slave.esc[base + UL_ECAT_SM_OFS_STATUS];
    }
};

/* 3.5.1 Writing partial SM0 does NOT trigger mailbox full. */
TEST_F(Mailbox, PartialSm0WriteDoesNotSetMbxFull) {
    uint8_t data[64] = {0};
    data[0] = 0x08; data[1] = 0x00; /* length = 8 */
    data[5] = UL_ECAT_MBX_TYPE_COE;
    fpwr(0x1000u, SM0_START, data, 64);
    EXPECT_EQ(sm_status(0) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
    EXPECT_EQ(capture.calls, 0);
}

/* 3.5.2 Writing full SM0 range (end at start+length) sets MBX_FULL and dispatches handler. */
TEST_F(Mailbox, FullSm0WriteDispatchesHandler) {
    uint8_t data[SM_LEN] = {0};
    data[0] = 0x02; data[1] = 0x00;            /* length = 2 */
    data[2] = 0x00; data[3] = 0x10;            /* address = 0x1000 */
    data[4] = 0x00;                            /* channel/prio */
    data[5] = UL_ECAT_MBX_TYPE_COE | (0x01 << 4);  /* type=CoE, counter=1 */
    data[6] = 0xAB; data[7] = 0xCD;            /* 2 bytes payload */

    uint8_t rx[1600], tx[1600];
    size_t rx_len = build_eth_fpwr(rx, sizeof(rx), SM0_START, data, SM_LEN);
    ASSERT_GT(rx_len, 0u);
    size_t tx_len = 0;
    ASSERT_EQ(ul_ecat_slave_controller_process_ethernet(&ctrl, rx, rx_len, tx, sizeof(tx), &tx_len), 0);

    EXPECT_EQ(capture.calls, 1);
    ASSERT_EQ(capture.last_frame.size(), (size_t)SM_LEN);
    EXPECT_EQ(capture.last_frame[0], 0x02u);  /* length LSB */
    EXPECT_EQ(capture.last_frame[5] & 0x0Fu, UL_ECAT_MBX_TYPE_COE);
    /* Handler returns; controller clears SM0 MBX_FULL. */
    EXPECT_EQ(sm_status(0) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
}

/* 3.5.3 mailbox_reply writes to SM1 and sets MBX_FULL; FPRD on full SM1 clears it. */
TEST_F(Mailbox, ReplySetsSm1Full_ReadClearsIt) {
    uint8_t reply[SM_LEN] = {0};
    reply[0] = 0x04; reply[1] = 0x00;  /* length = 4 */
    reply[5] = UL_ECAT_MBX_TYPE_COE | (0x02 << 4);
    reply[6] = 0xDE; reply[7] = 0xAD; reply[8] = 0xBE; reply[9] = 0xEF;

    ASSERT_EQ(ul_ecat_slave_controller_mailbox_reply(&ctrl, reply, SM_LEN), 0);
    EXPECT_NE(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);

    /* Master reads full SM1 -> bit clears. */
    uint8_t dg[512], pdu_out[512];
    size_t pdu_out_len = 0;
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000u, SM1_START, SM_LEN, 0, 0, nullptr);
    ASSERT_GT(enc, 0);
    ASSERT_EQ(ul_ecat_slave_process_pdu(&slave, dg, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
    uint16_t wkc = 0;
    std::vector<uint8_t> raw(SM_LEN);
    ASSERT_EQ(ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  &wkc, raw.data(), raw.size()), 0);
    EXPECT_GE(wkc, 1u);
    EXPECT_EQ(raw[0], 0x04u);
    EXPECT_EQ(raw[6], 0xDEu);
    EXPECT_EQ(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
}

/* 3.5.4 Partial SM1 read does NOT clear MBX_FULL. */
TEST_F(Mailbox, PartialSm1ReadDoesNotClearFull) {
    uint8_t reply[SM_LEN] = {0};
    reply[0] = 0x02; reply[1] = 0x00;
    ASSERT_EQ(ul_ecat_slave_controller_mailbox_reply(&ctrl, reply, SM_LEN), 0);
    EXPECT_NE(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);

    uint8_t dg[512], pdu_out[512];
    size_t pdu_out_len = 0;
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000u, SM1_START, 32u, 0, 0, nullptr);
    ASSERT_GT(enc, 0);
    ASSERT_EQ(ul_ecat_slave_process_pdu(&slave, dg, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
    EXPECT_NE(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
}

/* 3.5.5 Reply larger than SM1 is rejected. */
TEST_F(Mailbox, OversizedReplyRejected) {
    std::vector<uint8_t> big(SM_LEN + 1, 0);
    EXPECT_EQ(ul_ecat_slave_controller_mailbox_reply(&ctrl, big.data(), big.size()), -1);
    EXPECT_EQ(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
}

/* 3.5.6 Echo handler: handler replies with modified frame on incoming mailbox. */
TEST_F(Mailbox, EchoHandlerReplies) {
    struct EchoCtx {
        ul_ecat_slave_controller_t *ctrl;
    } ectx{&ctrl};

    ul_ecat_slave_controller_set_mailbox_handler(&ctrl,
        [](const uint8_t *frame, size_t len, void *ctx) {
            auto *e = static_cast<EchoCtx *>(ctx);
            std::vector<uint8_t> reply(frame, frame + len);
            /* Flip the counter nibble as an echo marker. */
            reply[5] = (uint8_t)((reply[5] & 0x0F) | (0x0A << 4));
            ul_ecat_slave_controller_mailbox_reply(e->ctrl, reply.data(), reply.size());
        }, &ectx);

    uint8_t data[SM_LEN] = {0};
    data[0] = 0x02; data[1] = 0x00;
    data[5] = UL_ECAT_MBX_TYPE_COE | (0x03 << 4);

    uint8_t rx[1600], tx[1600];
    size_t rx_len = build_eth_fpwr(rx, sizeof(rx), SM0_START, data, SM_LEN);
    ASSERT_GT(rx_len, 0u);
    size_t tx_len = 0;
    ASSERT_EQ(ul_ecat_slave_controller_process_ethernet(&ctrl, rx, rx_len, tx, sizeof(tx), &tx_len), 0);

    EXPECT_NE(sm_status(1) & UL_ECAT_SM_STAT_MBX_FULL, 0u);
    /* Read SM1 content back. */
    uint8_t dg[512], pdu_out[512];
    size_t pdu_out_len = 0;
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000u, SM1_START, SM_LEN, 0, 0, nullptr);
    ASSERT_GT(enc, 0);
    ASSERT_EQ(ul_ecat_slave_process_pdu(&slave, dg, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
    uint16_t wkc = 0;
    std::vector<uint8_t> raw(SM_LEN);
    ASSERT_EQ(ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  &wkc, raw.data(), raw.size()), 0);
    EXPECT_GE(wkc, 1u);
    EXPECT_EQ((raw[5] >> 4) & 0x0Fu, 0x0Au);
    EXPECT_EQ(raw[5] & 0x0Fu, UL_ECAT_MBX_TYPE_COE);
}
