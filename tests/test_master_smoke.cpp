#include <gtest/gtest.h>

extern "C" {
#include "ul_ecat_master.h"
}

TEST(Master, GetSlavesNonNull)
{
    ul_ecat_master_slaves_t *db = ul_ecat_get_master_slaves();
    ASSERT_NE(db, nullptr);
}
