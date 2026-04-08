/**
 * @file ul_ecat_slave_controller_lan9252.c
 * @brief LAN9252 backend: SPI sync into ESC mirror + optional PDO buffers (compiled with UL_ECAT_HAVE_LAN9252_CONTROLLER).
 */

#include "ul_ecat_slave_controller.h"

#include "lan9252.h"
#include "ul_ecat_esc.h"
#include "ul_ecat_esc_regs.h"

#include <errno.h>
#include <string.h>

static uint32_t read_u32_le_buf(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ul_ecat_slave_controller_poll_lan9252(ul_ecat_slave_controller_t *ctrl, uint32_t timeout_ms)
{
    ul_ecat_slave_t *s;
    uint8_t tmp[4];
    uint16_t st;
    uint32_t ev;
    int err;

    (void)timeout_ms;

    if (ctrl == NULL || ctrl->slave == NULL) {
        return -EINVAL;
    }
    s = ctrl->slave;

    err = lan9252_esc_read(UL_ECAT_ESC_REG_STADR, tmp, 2u);
    if (err != 0) {
        return err;
    }
    if (ul_ecat_esc_write(s->esc, UL_ECAT_ESC_REG_STADR, tmp, 2u) != 0) {
        return -EIO;
    }

    err = lan9252_esc_read(UL_ECAT_ESC_REG_ALSTAT, tmp, 2u);
    if (err != 0) {
        return err;
    }
    if (ul_ecat_esc_write(s->esc, UL_ECAT_ESC_REG_ALSTAT, tmp, 2u) != 0) {
        return -EIO;
    }

    err = lan9252_esc_read(UL_ECAT_ESC_REG_ALEVENT, tmp, 4u);
    if (err != 0) {
        return err;
    }
    if (ul_ecat_esc_write(s->esc, UL_ECAT_ESC_REG_ALEVENT, tmp, 4u) != 0) {
        return -EIO;
    }

    if (ctrl->pdram.in_len > 0u && ctrl->pdram.in_buf != NULL) {
        err = lan9252_pdram_read(ctrl->pdram.in_offset, ctrl->pdram.in_buf, ctrl->pdram.in_len);
        if (err != 0) {
            return err;
        }
    }
    if (ctrl->pdram.out_len > 0u && ctrl->pdram.out_buf != NULL) {
        err = lan9252_pdram_write(ctrl->pdram.out_offset, ctrl->pdram.out_buf, ctrl->pdram.out_len);
        if (err != 0) {
            return err;
        }
    }

    st = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_ALSTAT);
    ev = 0;
    if (ul_ecat_esc_read(s->esc, UL_ECAT_ESC_REG_ALEVENT, tmp, sizeof(tmp)) == 0) {
        ev = read_u32_le_buf(tmp);
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
