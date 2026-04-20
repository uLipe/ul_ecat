/**
 * @file test_od.cpp
 * @brief Object Dictionary lookup + read/write tests (no CoE/network involved).
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "ul_ecat_slave_od.h"
}

static uint32_t g_dev_type = 0x12345678u;
static char     g_dev_name[16] = "ul_ecat";
static uint8_t  g_id_count = 4u;
static uint32_t g_vendor   = 0x00000002u;
static uint32_t g_product  = 0x1C213052u;
static uint32_t g_param_rw = 0x0u;

static const ul_ecat_od_entry_t TEST_ENTRIES[] = {
    {0x1000, 0x00, UL_ECAT_OD_TYPE_U32,    UL_ECAT_OD_FLAG_R,  4,  &g_dev_type},
    {0x1008, 0x00, UL_ECAT_OD_TYPE_STRING, UL_ECAT_OD_FLAG_R, 16,  g_dev_name},
    {0x1018, 0x00, UL_ECAT_OD_TYPE_U8,     UL_ECAT_OD_FLAG_R,  1,  &g_id_count},
    {0x1018, 0x01, UL_ECAT_OD_TYPE_U32,    UL_ECAT_OD_FLAG_R,  4,  &g_vendor},
    {0x1018, 0x02, UL_ECAT_OD_TYPE_U32,    UL_ECAT_OD_FLAG_R,  4,  &g_product},
    {0x2000, 0x00, UL_ECAT_OD_TYPE_U32,    UL_ECAT_OD_FLAG_RW, 4,  &g_param_rw},
};

static const ul_ecat_od_table_t TEST_TABLE = {
    TEST_ENTRIES,
    sizeof(TEST_ENTRIES) / sizeof(TEST_ENTRIES[0]),
};

class Od : public ::testing::Test {
protected:
    void SetUp() override {
        g_param_rw = 0u;
        ul_ecat_od_set_table(&TEST_TABLE);
    }
    void TearDown() override {
        ul_ecat_od_set_table(nullptr);
    }
};

TEST_F(Od, LookupHitOnFirstEntry) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1000, 0x00);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->type, (uint8_t)UL_ECAT_OD_TYPE_U32);
}

TEST_F(Od, LookupHitWithSubindex) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1018, 0x02);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->index, 0x1018u);
    EXPECT_EQ(e->subindex, 0x02u);
}

TEST_F(Od, LookupMissUnknownIndex) {
    EXPECT_EQ(ul_ecat_od_lookup(0x9999, 0x00), nullptr);
}

TEST_F(Od, LookupMissUnknownSubindex) {
    EXPECT_NE(ul_ecat_od_lookup(0x1018, 0x00), nullptr);  /* subindex 0 exists */
    EXPECT_EQ(ul_ecat_od_lookup(0x1018, 0xFF), nullptr);  /* but 0xFF does not */
}

TEST_F(Od, IndexExists) {
    EXPECT_TRUE(ul_ecat_od_index_exists(0x1018));
    EXPECT_TRUE(ul_ecat_od_index_exists(0x2000));
    EXPECT_FALSE(ul_ecat_od_index_exists(0x9999));
}

TEST_F(Od, ReadU32Vendor) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1018, 0x01);
    ASSERT_NE(e, nullptr);
    uint8_t buf[8] = {0};
    int n = ul_ecat_od_read(e, buf, sizeof(buf));
    EXPECT_EQ(n, 4);
    uint32_t got = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                   ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    EXPECT_EQ(got, g_vendor);
}

TEST_F(Od, ReadStringDeviceName) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1008, 0x00);
    ASSERT_NE(e, nullptr);
    char buf[16] = {0};
    int n = ul_ecat_od_read(e, buf, sizeof(buf));
    EXPECT_EQ(n, 16);
    EXPECT_STREQ(buf, "ul_ecat");
}

TEST_F(Od, ReadDestTooSmall) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1018, 0x01);
    ASSERT_NE(e, nullptr);
    uint8_t buf[2] = {0};
    EXPECT_EQ(ul_ecat_od_read(e, buf, sizeof(buf)), UL_ECAT_OD_ERR_LENGTH);
}

TEST_F(Od, WriteToReadOnlyRejected) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x1000, 0x00);
    ASSERT_NE(e, nullptr);
    uint32_t v = 0xDEADBEEFu;
    EXPECT_EQ(ul_ecat_od_write(e, &v, sizeof(v)), UL_ECAT_OD_ERR_NOT_WRITABLE);
    EXPECT_EQ(g_dev_type, 0x12345678u);
}

TEST_F(Od, WriteThenReadRw) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x2000, 0x00);
    ASSERT_NE(e, nullptr);
    uint32_t v = 0xCAFEBABEu;
    EXPECT_EQ(ul_ecat_od_write(e, &v, sizeof(v)), UL_ECAT_OD_OK);
    uint8_t buf[4] = {0};
    EXPECT_EQ(ul_ecat_od_read(e, buf, sizeof(buf)), 4);
    uint32_t got = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                   ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    EXPECT_EQ(got, 0xCAFEBABEu);
}

TEST_F(Od, WriteOversizedRejected) {
    const ul_ecat_od_entry_t *e = ul_ecat_od_lookup(0x2000, 0x00);
    ASSERT_NE(e, nullptr);
    uint8_t big[8] = {0};
    EXPECT_EQ(ul_ecat_od_write(e, big, sizeof(big)), UL_ECAT_OD_ERR_LENGTH);
}

TEST_F(Od, NoTableInstalled) {
    ul_ecat_od_set_table(nullptr);
    EXPECT_EQ(ul_ecat_od_lookup(0x1000, 0x00), nullptr);
    EXPECT_FALSE(ul_ecat_od_index_exists(0x1018));
}
