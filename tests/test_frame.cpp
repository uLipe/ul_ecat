#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "ul_ecat_frame.h"
}

TEST(Frame, DgramEncodeRoundTrip)
{
    uint8_t buf[128];
    uint8_t payload[] = {0xAA, 0xBB};
    int n = ul_ecat_dgram_encode(buf, sizeof(buf), UL_ECAT_CMD_FPRD, 7,
                                 0x1000, 0x0130, 2u, 0u, 0u, payload);
    ASSERT_GT(n, 0);
    uint8_t cmd = 0;
    uint8_t idx = 0;
    uint16_t adp = 0;
    uint16_t ado = 0;
    uint16_t dlen = 0;
    uint16_t irq = 0;
    uint16_t wkc = 0;
    uint8_t out[8];
    int pr = ul_ecat_dgram_parse(buf, (size_t)n, 0, &cmd, &idx, &adp, &ado, &dlen, &irq, &wkc, out, sizeof(out));
    ASSERT_EQ(pr, 0);
    EXPECT_EQ(cmd, UL_ECAT_CMD_FPRD);
    EXPECT_EQ(idx, 7);
    EXPECT_EQ(adp, 0x1000u);
    EXPECT_EQ(ado, 0x0130u);
    EXPECT_EQ(dlen, 2u);
    EXPECT_GE(wkc, 0u);
    EXPECT_EQ(out[0], 0xAA);
    EXPECT_EQ(out[1], 0xBB);
}

TEST(Frame, PduCountTwoDatagrams)
{
    uint8_t a[64];
    uint8_t b[64];
    int na = ul_ecat_dgram_encode(a, sizeof(a), UL_ECAT_CMD_FPRD, 0, 0x1000, 0x0130, 2u, 0u, 0u, nullptr);
    int nb = ul_ecat_dgram_encode(b, sizeof(b), UL_ECAT_CMD_FPWR, 1, 0x1000, 0x0120, 2u, 0u, 0u,
                                  reinterpret_cast<const uint8_t *>("\x01\x00"));
    ASSERT_GT(na, 0);
    ASSERT_GT(nb, 0);
    uint8_t pdu[128];
    memcpy(pdu, a, (size_t)na);
    memcpy(pdu + na, b, (size_t)nb);
    int n = ul_ecat_pdu_count_datagrams(pdu, (size_t)na + (size_t)nb);
    EXPECT_EQ(n, 2);
}

TEST(Frame, EthBuildParse)
{
    uint8_t dst[6] = {1, 2, 3, 4, 5, 6};
    uint8_t src[6] = {6, 5, 4, 3, 2, 1};
    uint8_t dg[64];
    int enc = ul_ecat_dgram_encode(dg, sizeof(dg), UL_ECAT_CMD_FPWR, 0, 0x1000, 0x0120, 2u, 0u, 0u,
                                   reinterpret_cast<const uint8_t *>("\x02\x00"));
    ASSERT_GT(enc, 0);
    uint8_t frame[256];
    ssize_t flen = ul_ecat_build_eth_frame(dst, src, dg, (size_t)enc, frame, sizeof(frame));
    ASSERT_GT(flen, 0);
    const uint8_t *pdu = nullptr;
    size_t pdu_len = 0;
    ASSERT_EQ(ul_ecat_parse_eth_frame(frame, (size_t)flen, &pdu, &pdu_len), 0);
    EXPECT_EQ(static_cast<size_t>(enc), pdu_len);
    EXPECT_EQ(0, memcmp(pdu, dg, (size_t)enc));
}
