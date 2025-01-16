/**
 * Negative / edge-case tests for EtherCAT frame and PDU parsing.
 */
#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "ul_ecat_frame.h"
}

TEST(FrameNegative, ParseEthWrongEtherType)
{
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x00;
    const uint8_t *pdu = nullptr;
    size_t plen = 0;
    EXPECT_NE(ul_ecat_parse_eth_frame(frame, 32, &pdu, &plen), 0);
}

TEST(FrameNegative, ParseEthFrameTooShort)
{
    uint8_t frame[20];
    memset(frame, 0, sizeof(frame));
    frame[12] = (uint8_t)((UL_ECAT_ETHERTYPE >> 8) & 0xFF);
    frame[13] = (uint8_t)(UL_ECAT_ETHERTYPE & 0xFF);
    const uint8_t *pdu = nullptr;
    size_t plen = 0;
    EXPECT_NE(ul_ecat_parse_eth_frame(frame, 20, &pdu, &plen), 0);
}

TEST(FrameNegative, PduTruncatedMidDatagram)
{
    uint8_t dg[64];
    int n = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000, 0x0130, 4u, 0u, 0u, nullptr);
    ASSERT_GT(n, 0);
    /* Chop before WKC */
    EXPECT_LT(ul_ecat_pdu_count_datagrams(dg, (size_t)n - 1), 0);
}

TEST(FrameNegative, PduGarbageTail)
{
    uint8_t dg[64];
    int n = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPRD, 0, 0x1000, 0x0130, 2u, 0u, 0u, nullptr);
    ASSERT_GT(n, 0);
    uint8_t buf[80];
    memcpy(buf, dg, (size_t)n);
    buf[n] = 0xAB;
    EXPECT_LT(ul_ecat_pdu_count_datagrams(buf, (size_t)n + 1), 0);
}

TEST(FrameGolden, ApwrEncodeMatchesExpectedLayout)
{
    uint8_t payload[] = {0x34, 0x12};
    uint8_t dg[32];
    int n = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_APWR, 3, 0x0000, 0x0010, 2u, 0u, 0u, payload);
    ASSERT_EQ(n, 14);
    EXPECT_EQ(dg[0], UL_ECAT_CMD_APWR);
    EXPECT_EQ(dg[1], 3);
    EXPECT_EQ(dg[2], 0x00);
    EXPECT_EQ(dg[3], 0x00);
    EXPECT_EQ(dg[4], 0x10);
    EXPECT_EQ(dg[5], 0x00);
    EXPECT_EQ(dg[10], 0x34);
    EXPECT_EQ(dg[11], 0x12);
    uint16_t wkc = 0;
    uint8_t out[4];
    ASSERT_EQ(ul_ecat_dgram_parse(dg, (size_t)n, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &wkc, out, sizeof(out)), 0);
    EXPECT_EQ(wkc, 0u);
}

TEST(FrameGolden, MultiDgramParseSecond)
{
    uint8_t a[64], b[64];
    int na = ul_ecat_dgram_encode(a, sizeof(a), UL_ECAT_CMD_FPRD, 0, 0x1000, 0x0012, 4u, 0u, 0u, nullptr);
    int nb = ul_ecat_dgram_encode(b, sizeof(b), UL_ECAT_CMD_FPRD, 1, 0x1000, 0x0016, 4u, 0u, 0u, nullptr);
    ASSERT_GT(na, 0);
    ASSERT_GT(nb, 0);
    uint8_t pdu[128];
    memcpy(pdu, a, (size_t)na);
    memcpy(pdu + na, b, (size_t)nb);
    ASSERT_EQ(ul_ecat_pdu_count_datagrams(pdu, (size_t)na + (size_t)nb), 2);
    uint16_t ado = 0;
    ASSERT_EQ(ul_ecat_dgram_parse(pdu, (size_t)na + (size_t)nb, 1, nullptr, nullptr, nullptr, &ado, nullptr, nullptr, nullptr, nullptr, 0), 0);
    EXPECT_EQ(ado, 0x0016u);
}
