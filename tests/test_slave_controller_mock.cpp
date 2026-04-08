/**
 * @file test_slave_controller_mock.cpp
 * @brief LAN9252 controller chain tests: in-memory HAL mock, poll sync, PDO, callbacks.
 */

#include <gtest/gtest.h>

#include <cstring>

#include "lan9252_full_mock.h"
#include "ul_ecat_esc_regs.h"

extern "C" {
#include "ul_ecat_esc.h"
}
#include "ul_ecat_slave.h"
#include "ul_ecat_slave_controller.h"

namespace {

void seed_mock_esc_registers(uint16_t stadr, uint16_t alstat, uint32_t alevent)
{
    uint8_t *esc = lan9252_mock_esc_core();
    esc[UL_ECAT_ESC_REG_STADR] = (uint8_t)(stadr & 0xFFu);
    esc[UL_ECAT_ESC_REG_STADR + 1] = (uint8_t)((stadr >> 8) & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALSTAT] = (uint8_t)(alstat & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALSTAT + 1] = (uint8_t)((alstat >> 8) & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALEVENT] = (uint8_t)(alevent & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALEVENT + 1] = (uint8_t)((alevent >> 8) & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALEVENT + 2] = (uint8_t)((alevent >> 16) & 0xFFu);
    esc[UL_ECAT_ESC_REG_ALEVENT + 3] = (uint8_t)((alevent >> 24) & 0xFFu);
}

} // namespace

TEST(Lan9252ControllerMock, PollCopiesEscRegistersIntoMirror)
{
    lan9252_mock_reset();

    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    ul_ecat_slave_identity_t id = {
        .vendor_id = 0x11223344u,
        .product_code = 0x55667788u,
        .revision = 0x00010203u,
        .serial = 0xAABBCCDDu,
    };
    uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

    seed_mock_esc_registers(0x1001u, 0x0200u, 0xCAFEBABEu);

    ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id), 0);
    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);

    uint8_t tmp[4];
    ASSERT_EQ(ul_ecat_esc_read(slave.esc, UL_ECAT_ESC_REG_STADR, tmp, 2u), 0);
    EXPECT_EQ((uint16_t)tmp[0] | ((uint16_t)tmp[1] << 8), 0x1001u);
    ASSERT_EQ(ul_ecat_esc_read(slave.esc, UL_ECAT_ESC_REG_ALSTAT, tmp, 2u), 0);
    EXPECT_EQ((uint16_t)tmp[0] | ((uint16_t)tmp[1] << 8), 0x0200u);
    ASSERT_EQ(ul_ecat_esc_read(slave.esc, UL_ECAT_ESC_REG_ALEVENT, tmp, 4u), 0);
    EXPECT_EQ((uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24),
              0xCAFEBABEu);
}

TEST(Lan9252ControllerMock, SimulatedChipEscWriteThenPollSyncsMirror)
{
    lan9252_mock_reset();

    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    ul_ecat_slave_identity_t id = {
        .vendor_id = 1u,
        .product_code = 2u,
        .revision = 3u,
        .serial = 4u,
    };
    uint8_t mac[6] = {0x02, 0, 0, 0, 0, 0x01};

    ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id), 0);

    /* Simula o ESC no chip após um datagrama APWR (ou lógica interna) — só o mock mudou. */
    seed_mock_esc_registers(0x00ABu, 0x0008u, 0x00010203u);

    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);

    EXPECT_EQ(ul_ecat_esc_read_u16_le(slave.esc, UL_ECAT_ESC_REG_STADR), 0x00ABu);
    EXPECT_EQ(ul_ecat_esc_read_u16_le(slave.esc, UL_ECAT_ESC_REG_ALSTAT), 0x0008u);
}

TEST(Lan9252ControllerMock, PdramInCopiesFromMockToBuffers)
{
    lan9252_mock_reset();

    uint8_t *pd = lan9252_mock_pdram();
    std::memset(pd, 0xEE, 32u);
    pd[0] = 0x11u;
    pd[1] = 0x22u;
    pd[2] = 0x33u;
    pd[3] = 0x44u;

    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    ul_ecat_slave_identity_t id = {.vendor_id = 1u, .product_code = 2u, .revision = 3u, .serial = 4u};
    uint8_t mac[6] = {};

    uint8_t in_buf[8] = {};
    ul_ecat_slave_controller_pdram_cfg_t cfg = {};
    cfg.in_offset = 0x1000u;
    cfg.in_len = 4u;
    cfg.in_buf = in_buf;

    ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id), 0);
    ul_ecat_slave_controller_set_pdram(&ctrl, &cfg);
    seed_mock_esc_registers(0, 0x0001u, 0);

    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);

    EXPECT_EQ(in_buf[0], 0x11u);
    EXPECT_EQ(in_buf[1], 0x22u);
    EXPECT_EQ(in_buf[2], 0x33u);
    EXPECT_EQ(in_buf[3], 0x44u);
}

TEST(Lan9252ControllerMock, PdramOutWritesMockFromBuffers)
{
    lan9252_mock_reset();

    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    ul_ecat_slave_identity_t id = {.vendor_id = 1u, .product_code = 2u, .revision = 3u, .serial = 4u};
    uint8_t mac[6] = {};

    uint8_t out_buf[4] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    ul_ecat_slave_controller_pdram_cfg_t cfg = {};
    cfg.out_offset = 0x1100u; /* ETG 0x1100 -> índice 0x100 no buffer PRAM mock */
    cfg.out_len = 4u;
    cfg.out_buf = out_buf;

    ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id), 0);
    ul_ecat_slave_controller_set_pdram(&ctrl, &cfg);
    seed_mock_esc_registers(0, 0x0001u, 0);

    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);

    uint8_t *pd = lan9252_mock_pdram();
    size_t idx = 0x1100u - 0x1000u;
    EXPECT_EQ(pd[idx + 0], 0xDEu);
    EXPECT_EQ(pd[idx + 1], 0xADu);
    EXPECT_EQ(pd[idx + 2], 0xBEu);
    EXPECT_EQ(pd[idx + 3], 0xEFu);
}

struct CallbackCtx {
    unsigned al_calls = 0;
    uint16_t last_al = 0;
    unsigned ev_calls = 0;
    uint32_t last_ev = 0;
};

static void on_al_status(uint16_t st, void *user_ctx)
{
    auto *c = static_cast<CallbackCtx *>(user_ctx);
    c->al_calls++;
    c->last_al = st;
}

static void on_esc_event(uint32_t ev, void *user_ctx)
{
    auto *c = static_cast<CallbackCtx *>(user_ctx);
    c->ev_calls++;
    c->last_ev = ev;
}

TEST(Lan9252ControllerMock, CallbacksFireWhenMockRegistersChange)
{
    lan9252_mock_reset();

    ul_ecat_slave_t slave{};
    ul_ecat_slave_controller_t ctrl{};
    ul_ecat_slave_identity_t id = {.vendor_id = 1u, .product_code = 2u, .revision = 3u, .serial = 4u};
    uint8_t mac[6] = {};

    CallbackCtx ctx{};

    ASSERT_EQ(ul_ecat_slave_controller_init(&ctrl, &slave, UL_ECAT_SLAVE_BACKEND_LAN9252_SPI, mac, &id), 0);
    ul_ecat_slave_controller_set_callbacks(&ctrl, on_al_status, on_esc_event, &ctx);

    /* Mirror init: AL INIT = 0x0001; primeiro poll com o mesmo valor no mock — sem callback. */
    seed_mock_esc_registers(0, 0x0001u, 0u);
    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);
    EXPECT_EQ(ctx.al_calls, 0u);
    EXPECT_EQ(ctx.ev_calls, 0u);

    seed_mock_esc_registers(0, 0x0003u, 0x00000040u);
    ASSERT_EQ(ul_ecat_slave_controller_poll(&ctrl, 0), 0);
    EXPECT_EQ(ctx.al_calls, 1u);
    EXPECT_EQ(ctx.last_al, 0x0003u);
    EXPECT_EQ(ctx.ev_calls, 1u);
    EXPECT_EQ(ctx.last_ev, 0x00000040u);
}
