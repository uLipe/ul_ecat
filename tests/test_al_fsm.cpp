/**
 * @file test_al_fsm.cpp
 * @brief Slave AL state machine: valid/invalid transitions via FPWR on ALCTL.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "ul_ecat_al.h"
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_frame.h"
#include "ul_ecat_slave.h"
}

static const ul_ecat_slave_identity_t TEST_ID = {0x02u, 0x1C213052u, 1u, 1u};
static const uint8_t TEST_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

class AlFsm : public ::testing::Test {
protected:
    ul_ecat_slave_t slave{};

    void SetUp() override {
        ul_ecat_slave_init(&slave, TEST_MAC, &TEST_ID);
        assign_station(0x1000u);
    }

    void assign_station(uint16_t station) {
        uint8_t buf[2] = {(uint8_t)(station & 0xFF), (uint8_t)(station >> 8)};
        fpwr(station, UL_ECAT_ESC_REG_STADR, buf, 2, true);
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

    void request_state(uint8_t state) {
        uint16_t w = ul_ecat_al_control_word(state, 0);
        uint8_t buf[2] = {(uint8_t)(w & 0xFF), (uint8_t)(w >> 8)};
        EXPECT_GE(fpwr(0x1000u, UL_ECAT_ESC_REG_ALCTL, buf, 2), 1u);
    }

    void ack_error() {
        uint8_t cur = slave.esc[UL_ECAT_ESC_REG_ALSTAT] & UL_ECAT_AL_MASK_STATE;
        uint16_t w = ul_ecat_al_control_word(cur, 1);
        uint8_t buf[2] = {(uint8_t)(w & 0xFF), (uint8_t)(w >> 8)};
        EXPECT_GE(fpwr(0x1000u, UL_ECAT_ESC_REG_ALCTL, buf, 2), 1u);
    }

    uint8_t current_state() {
        return slave.esc[UL_ECAT_ESC_REG_ALSTAT] & UL_ECAT_AL_MASK_STATE;
    }

    bool error_indicated() {
        return (slave.esc[UL_ECAT_ESC_REG_ALSTAT] & 0x10u) != 0;
    }

    uint16_t status_code() {
        return (uint16_t)slave.esc[UL_ECAT_ESC_REG_ALSTACODE] |
               ((uint16_t)slave.esc[UL_ECAT_ESC_REG_ALSTACODE + 1] << 8);
    }
};

TEST_F(AlFsm, InitialStateIsInit) {
    EXPECT_EQ(current_state(), 1u);
    EXPECT_FALSE(error_indicated());
    EXPECT_EQ(status_code(), 0u);
}

TEST_F(AlFsm, InitToPreop) {
    request_state(2);
    EXPECT_EQ(current_state(), 2u);
    EXPECT_FALSE(error_indicated());
}

TEST_F(AlFsm, PreopToSafeop) {
    request_state(2);
    request_state(4);
    EXPECT_EQ(current_state(), 4u);
}

TEST_F(AlFsm, SafeopToOp) {
    request_state(2);
    request_state(4);
    request_state(8);
    EXPECT_EQ(current_state(), 8u);
}

TEST_F(AlFsm, FullForwardThenBackward) {
    request_state(2);
    request_state(4);
    request_state(8);
    EXPECT_EQ(current_state(), 8u);

    request_state(4);
    EXPECT_EQ(current_state(), 4u);
    request_state(2);
    EXPECT_EQ(current_state(), 2u);
    request_state(1);
    EXPECT_EQ(current_state(), 1u);
}

TEST_F(AlFsm, AnyToInitAlwaysValid) {
    request_state(2);
    request_state(4);
    EXPECT_EQ(current_state(), 4u);
    request_state(1);
    EXPECT_EQ(current_state(), 1u);
}

TEST_F(AlFsm, OpToInitDirectValid) {
    request_state(2);
    request_state(4);
    request_state(8);
    request_state(1);
    EXPECT_EQ(current_state(), 1u);
    EXPECT_FALSE(error_indicated());
}

TEST_F(AlFsm, InitToOpInvalid) {
    request_state(8);
    EXPECT_EQ(current_state(), 1u);
    EXPECT_TRUE(error_indicated());
    EXPECT_EQ(status_code(), UL_ECAT_AL_ERR_INVALID_STATE_CHANGE);
}

TEST_F(AlFsm, InitToSafeopInvalid) {
    request_state(4);
    EXPECT_EQ(current_state(), 1u);
    EXPECT_TRUE(error_indicated());
    EXPECT_EQ(status_code(), UL_ECAT_AL_ERR_INVALID_STATE_CHANGE);
}

TEST_F(AlFsm, PreopToOpInvalid) {
    request_state(2);
    request_state(8);
    EXPECT_EQ(current_state(), 2u);
    EXPECT_TRUE(error_indicated());
    EXPECT_EQ(status_code(), UL_ECAT_AL_ERR_INVALID_STATE_CHANGE);
}

TEST_F(AlFsm, BootNotSupported) {
    request_state(3);
    EXPECT_EQ(current_state(), 1u);
    EXPECT_TRUE(error_indicated());
    EXPECT_EQ(status_code(), UL_ECAT_AL_ERR_BOOTSTRAP_NOT_SUPPORTED);
}

TEST_F(AlFsm, AckClearsError) {
    request_state(8);
    EXPECT_TRUE(error_indicated());
    ack_error();
    EXPECT_FALSE(error_indicated());
    EXPECT_EQ(status_code(), 0u);
    EXPECT_EQ(current_state(), 1u);
}

TEST_F(AlFsm, SameStateRequestIsNoop) {
    request_state(2);
    EXPECT_EQ(current_state(), 2u);
    request_state(2);
    EXPECT_EQ(current_state(), 2u);
    EXPECT_FALSE(error_indicated());
}
