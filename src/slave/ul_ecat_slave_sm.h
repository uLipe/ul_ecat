/**
 * @file ul_ecat_slave_sm.h
 * @brief SyncManager helpers: read config from ESC mirror, validate (internal header).
 */

#ifndef UL_ECAT_SLAVE_SM_H
#define UL_ECAT_SLAVE_SM_H

#include "ul_ecat_esc_regs.h"
#include "ul_ecat_slave.h"

#include <stdint.h>
#include <string.h>

typedef struct {
	uint16_t start_addr;
	uint16_t length;
	uint8_t  control;
	uint8_t  status;
	uint8_t  activate;
	uint8_t  pdi_ctrl;
} ul_ecat_sm_config_t;

static inline uint16_t ul_ecat_sm_base(unsigned sm_index)
{
	return (uint16_t)(UL_ECAT_ESC_REG_SM0 + sm_index * UL_ECAT_ESC_SM_SIZE);
}

static inline void ul_ecat_sm_read(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE],
				   unsigned sm_index, ul_ecat_sm_config_t *out)
{
	uint16_t base = ul_ecat_sm_base(sm_index);
	memset(out, 0, sizeof(*out));
	if ((size_t)base + UL_ECAT_ESC_SM_SIZE > UL_ECAT_SLAVE_ESC_SIZE) {
		return;
	}
	const uint8_t *p = esc + base;
	out->start_addr = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
	out->length     = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
	out->control    = p[4];
	out->status     = p[5];
	out->activate   = p[6];
	out->pdi_ctrl   = p[7];
}

static inline int ul_ecat_sm_is_active(const ul_ecat_sm_config_t *sm)
{
	return (sm->activate & 0x01u) != 0;
}

static inline int ul_ecat_sm_is_mailbox(const ul_ecat_sm_config_t *sm)
{
	return (sm->control & UL_ECAT_SM_CTRL_MODE_MASK) == UL_ECAT_SM_CTRL_MODE_MBX;
}

/**
 * Validate that SM0 (mailbox write) and SM1 (mailbox read) are configured
 * with non-zero length, mailbox mode, and activated.
 * @return 0 if valid, -1 if not.
 */
static inline int ul_ecat_sm_validate_mailbox_pair(const uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE])
{
	ul_ecat_sm_config_t sm0, sm1;
	ul_ecat_sm_read(esc, 0, &sm0);
	ul_ecat_sm_read(esc, 1, &sm1);

	if (!ul_ecat_sm_is_active(&sm0) || sm0.length == 0u || !ul_ecat_sm_is_mailbox(&sm0)) {
		return -1;
	}
	if (!ul_ecat_sm_is_active(&sm1) || sm1.length == 0u || !ul_ecat_sm_is_mailbox(&sm1)) {
		return -1;
	}
	return 0;
}

#endif /* UL_ECAT_SLAVE_SM_H */
