/**
 * @file ul_ecat_slave_controller.c
 * @brief Slave controller: software Ethernet delegation + callback dispatch from mirror.
 */

#include "ul_ecat_slave_controller.h"

#include "ul_ecat_esc.h"
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_slave.h"
#include "ul_ecat_slave_mailbox.h"
#include "ul_ecat_slave_sm.h"

#include <errno.h>
#include <string.h>

#ifdef UL_ECAT_HAVE_LAN9252_CONTROLLER
extern int ul_ecat_slave_controller_poll_lan9252(ul_ecat_slave_controller_t *ctrl, uint32_t timeout_ms);
#endif

static uint32_t read_u32_le_buf(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int poll_software(ul_ecat_slave_controller_t *ctrl)
{
    ul_ecat_slave_t *s = ctrl->slave;
    uint16_t st = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_ALSTAT);
    uint8_t evb[4];
    uint32_t ev = 0;
    if (ul_ecat_esc_read(s->esc, UL_ECAT_ESC_REG_ALEVENT, evb, sizeof(evb)) == 0) {
        ev = read_u32_le_buf(evb);
    }

    if (ctrl->on_al_status != NULL && st != ctrl->last_al_status) {
        ctrl->on_al_status(st, ctrl->user_ctx);
    }
    if (ctrl->on_esc_event != NULL && ev != ctrl->last_al_event) {
        ctrl->on_esc_event(ev, ctrl->user_ctx);
    }
    ctrl->last_al_status = st;
    ctrl->last_al_event = ev;
    return 0;
}

int ul_ecat_slave_controller_init(ul_ecat_slave_controller_t *ctrl,
                                  ul_ecat_slave_t *slave,
                                  ul_ecat_slave_backend_t backend,
                                  const uint8_t mac[6],
                                  const ul_ecat_slave_identity_t *id)
{
    if (ctrl == NULL || slave == NULL || id == NULL || mac == NULL) {
        return -EINVAL;
    }
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->slave = slave;
    ctrl->backend = backend;
    ul_ecat_slave_init(slave, mac, id);
    ctrl->last_al_status = ul_ecat_esc_read_u16_le(slave->esc, UL_ECAT_ESC_REG_ALSTAT);
    {
        uint8_t evb[4];
        if (ul_ecat_esc_read(slave->esc, UL_ECAT_ESC_REG_ALEVENT, evb, sizeof(evb)) == 0) {
            ctrl->last_al_event = read_u32_le_buf(evb);
        }
    }
    return 0;
}

void ul_ecat_slave_controller_set_callbacks(ul_ecat_slave_controller_t *ctrl,
                                            ul_ecat_slave_on_al_status_t on_al_status,
                                            ul_ecat_slave_on_esc_event_t on_esc_event,
                                            void *user_ctx)
{
    if (ctrl == NULL) {
        return;
    }
    ctrl->on_al_status = on_al_status;
    ctrl->on_esc_event = on_esc_event;
    ctrl->user_ctx = user_ctx;
}

void ul_ecat_slave_controller_set_mailbox_handler(ul_ecat_slave_controller_t *ctrl,
                                                   ul_ecat_slave_on_mailbox_rx_t cb,
                                                   void *user_ctx)
{
    if (ctrl == NULL) {
        return;
    }
    ctrl->on_mailbox_rx = cb;
    ctrl->mailbox_ctx = user_ctx;
}

int ul_ecat_slave_controller_mailbox_reply(ul_ecat_slave_controller_t *ctrl,
                                           const uint8_t *frame, size_t len)
{
    if (ctrl == NULL || ctrl->slave == NULL) {
        return -EINVAL;
    }
    return ul_ecat_slave_mailbox_write_sm1(ctrl->slave, frame, len);
}

static void dispatch_mailbox_if_ready(ul_ecat_slave_controller_t *ctrl)
{
    if (ctrl->on_mailbox_rx == NULL) {
        return;
    }
    ul_ecat_slave_t *s = ctrl->slave;
    ul_ecat_sm_config_t sm0;
    ul_ecat_sm_read(s->esc, 0, &sm0);
    if (!ul_ecat_sm_is_active(&sm0) || !ul_ecat_sm_is_mailbox(&sm0) || sm0.length == 0u) {
        return;
    }
    uint8_t status = s->esc[ul_ecat_sm_base(0) + UL_ECAT_SM_OFS_STATUS];
    if ((status & UL_ECAT_SM_STAT_MBX_FULL) == 0u) {
        return;
    }
    if ((size_t)sm0.start_addr + sm0.length > UL_ECAT_SLAVE_ESC_SIZE) {
        return;
    }
    ctrl->on_mailbox_rx(s->esc + sm0.start_addr, sm0.length, ctrl->mailbox_ctx);
    s->esc[ul_ecat_sm_base(0) + UL_ECAT_SM_OFS_STATUS] &= (uint8_t)~UL_ECAT_SM_STAT_MBX_FULL;
}

void ul_ecat_slave_controller_set_pdram(ul_ecat_slave_controller_t *ctrl,
                                        const ul_ecat_slave_controller_pdram_cfg_t *cfg)
{
    if (ctrl == NULL) {
        return;
    }
    if (cfg == NULL) {
        memset(&ctrl->pdram, 0, sizeof(ctrl->pdram));
        return;
    }
    ctrl->pdram = *cfg;
}

int ul_ecat_slave_controller_process_ethernet(ul_ecat_slave_controller_t *ctrl,
                                              const uint8_t *rx, size_t rx_len,
                                              uint8_t *tx, size_t tx_cap, size_t *tx_len)
{
    if (ctrl == NULL || ctrl->slave == NULL || tx_len == NULL) {
        return -EINVAL;
    }
    if (ctrl->backend != UL_ECAT_SLAVE_BACKEND_SOFTWARE_ETHERNET) {
        return -ENOTSUP;
    }
    int r = ul_ecat_slave_process_ethernet(ctrl->slave, rx, rx_len, tx, tx_cap, tx_len);
    if (r == 0) {
        dispatch_mailbox_if_ready(ctrl);
    }
    return r;
}

int ul_ecat_slave_controller_poll(ul_ecat_slave_controller_t *ctrl, uint32_t timeout_ms)
{
    if (ctrl == NULL || ctrl->slave == NULL) {
        return -EINVAL;
    }
    (void)timeout_ms;

    if (ctrl->backend == UL_ECAT_SLAVE_BACKEND_SOFTWARE_ETHERNET) {
        return poll_software(ctrl);
    }
    if (ctrl->backend == UL_ECAT_SLAVE_BACKEND_LAN9252_SPI) {
#ifdef UL_ECAT_HAVE_LAN9252_CONTROLLER
        return ul_ecat_slave_controller_poll_lan9252(ctrl, timeout_ms);
#else
        return -ENOTSUP;
#endif
    }
    return -EINVAL;
}
