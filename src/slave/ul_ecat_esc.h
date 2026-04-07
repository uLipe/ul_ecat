/**
 * @file ul_ecat_esc.h
 * @brief Internal ESC helpers (not installed).
 */

#ifndef UL_ECAT_ESC_H
#define UL_ECAT_ESC_H

#include "ul_ecat_slave.h"

void ul_ecat_esc_apply_identity(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], const ul_ecat_slave_identity_t *id);

void ul_ecat_esc_set_al_status_init(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE]);

int ul_ecat_esc_read(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado, void *dst, size_t len);

int ul_ecat_esc_write(uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado, const void *src, size_t len);

uint16_t ul_ecat_esc_read_u16_le(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE], uint16_t ado);

#endif
