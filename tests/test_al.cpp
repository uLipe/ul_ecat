#include <gtest/gtest.h>

extern "C" {
#include "ul_ecat_al.h"
}

TEST(Al, ControlWordRequestOnly)
{
    uint16_t w = ul_ecat_al_control_word(2, 0);
    EXPECT_EQ(w, 0x0002u);
}

TEST(Al, ControlWordWithAck)
{
    uint16_t w = ul_ecat_al_control_word(8, 1);
    EXPECT_EQ(w, 0x0018u);
}

TEST(Al, StatusStateNibble)
{
    EXPECT_EQ(ul_ecat_al_status_state(0x0002), 2u);
    EXPECT_EQ(ul_ecat_al_status_state(0x0008), 8u);
}

TEST(Al, StatusErrorBit)
{
    EXPECT_EQ(ul_ecat_al_status_error_indicated(0x0012), 1);
    EXPECT_EQ(ul_ecat_al_status_error_indicated(0x0002), 0);
}
