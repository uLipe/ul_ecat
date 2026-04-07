/**
 * @file ul_ecat_esc.c
 * @brief Byte-addressable ESC register mirror (minimal subset).
 */

#include "ul_ecat_esc.h"

#include <string.h>

static void w32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

void ul_ecat_esc_apply_identity(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], const ul_ecat_slave_identity_t *id)
{
    w32_le(esc + UL_ECAT_ESC_REG_VENDOR, id->vendor_id);
    w32_le(esc + UL_ECAT_ESC_REG_PRODUCT, id->product_code);
    w32_le(esc + UL_ECAT_ESC_REG_REV, id->revision);
    w32_le(esc + UL_ECAT_ESC_REG_SERIAL, id->serial);
}

void ul_ecat_esc_set_al_status_init(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE])
{
    /* State INIT = 0x1 in low nibble; no error bit */
    esc[UL_ECAT_ESC_REG_ALSTAT] = 0x01u;
    esc[UL_ECAT_ESC_REG_ALSTAT + 1] = 0x00u;
}

int ul_ecat_esc_read(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado, void *dst, size_t len)
{
    if ((size_t)ado + len > UL_ECAT_SLAVE_ESC_SIZE || len == 0u) {
        return -1;
    }
    memcpy(dst, esc + ado, len);
    return 0;
}

int ul_ecat_esc_write(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado, const void *src, size_t len)
{
    if ((size_t)ado + len > UL_ECAT_SLAVE_ESC_SIZE || len == 0u) {
        return -1;
    }
    memcpy(esc + ado, src, len);
    return 0;
}

uint16_t ul_ecat_esc_read_u16_le(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado)
{
    uint16_t v = 0;
    if ((size_t)ado + 2u > UL_ECAT_SLAVE_ESC_SIZE) {
        return 0;
    }
    v = (uint16_t)esc[ado] | ((uint16_t)esc[ado + 1] << 8);
    return v;
}
