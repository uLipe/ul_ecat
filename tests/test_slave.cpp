#include <gtest/gtest.h>

#include <cstring>

#include "ul_ecat_frame.h"
#include "ul_ecat_slave.h"

TEST(SlaveIdentity, FprdVendorAfterApwrStation)
{
    ul_ecat_slave_t slave{};
    ul_ecat_slave_identity_t id = {
        .vendor_id = 0x11223344u,
        .product_code = 0x55667788u,
        .revision = 0x00010203u,
        .serial = 0xAABBCCDDu,
    };
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    ul_ecat_slave_init(&slave, mac, &id);

    /* APWR: position 0, ADO STADR, 2 bytes station 0x1000 */
    uint8_t stadr[2] = {0x00, 0x10};
    uint8_t dg_apwr[64];
    int enc = ul_ecat_dgram_encode(dg_apwr, sizeof(dg_apwr), UL_ECAT_CMD_APWR, 0, 0u, UL_ECAT_ESC_REG_STADR, 2u, 0u, 0u,
                                   stadr);
    ASSERT_GT(enc, 0);

    uint8_t pdu_out[512];
    size_t pdu_out_len = 0;
    ASSERT_EQ(ul_ecat_slave_process_pdu(&slave, dg_apwr, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
    ASSERT_GT(pdu_out_len, 0u);

    uint16_t wkc = 0;
    uint8_t raw[4];
    ASSERT_EQ(ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &wkc,
                                  raw, sizeof(raw)),
              0);
    EXPECT_GE(wkc, 1u);

    /* FPRD vendor @ 0x12, station 0x1000 */
    uint8_t dg_fprd[64];
    enc = ul_ecat_dgram_encode(dg_fprd, sizeof(dg_fprd), UL_ECAT_CMD_FPRD, 0, 0x1000u, UL_ECAT_ESC_REG_VENDOR, 4u, 0u,
                              0u, nullptr);
    ASSERT_GT(enc, 0);

    ASSERT_EQ(ul_ecat_slave_process_pdu(&slave, dg_fprd, (size_t)enc, pdu_out, sizeof(pdu_out), &pdu_out_len), 0);
    ASSERT_EQ(ul_ecat_dgram_parse(pdu_out, pdu_out_len, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &wkc,
                                  raw, sizeof(raw)),
              0);
    EXPECT_GE(wkc, 1u);
    EXPECT_EQ((uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24),
              id.vendor_id);
}
