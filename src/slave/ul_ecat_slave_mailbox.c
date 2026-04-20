/**
 * @file ul_ecat_slave_mailbox.c
 * @brief Mailbox transport: SM0 (master->slave) / SM1 (slave->master) handshake.
 */

#include "ul_ecat_slave_mailbox.h"

#include "ul_ecat_esc.h"
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_slave_sm.h"

#include <string.h>

void ul_ecat_mbx_header_decode(const uint8_t *frame, ul_ecat_mbx_header_t *out)
{
	out->length   = (uint16_t)frame[0] | ((uint16_t)frame[1] << 8);
	out->address  = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
	out->channel  = frame[4] & 0x3Fu;
	out->priority = (frame[4] >> 6) & 0x03u;
	out->type     = frame[5] & 0x0Fu;
	out->counter  = (frame[5] >> 4) & 0x0Fu;
}

void ul_ecat_mbx_header_encode(uint8_t *frame, const ul_ecat_mbx_header_t *hdr)
{
	frame[0] = (uint8_t)(hdr->length & 0xFFu);
	frame[1] = (uint8_t)((hdr->length >> 8) & 0xFFu);
	frame[2] = (uint8_t)(hdr->address & 0xFFu);
	frame[3] = (uint8_t)((hdr->address >> 8) & 0xFFu);
	frame[4] = (uint8_t)((hdr->channel & 0x3Fu) | ((hdr->priority & 0x03u) << 6));
	frame[5] = (uint8_t)((hdr->type & 0x0Fu) | ((hdr->counter & 0x0Fu) << 4));
}

static void sm_set_status_bit(uint8_t *esc, unsigned sm_index, uint8_t bit_mask)
{
	uint16_t base = ul_ecat_sm_base(sm_index);
	esc[base + UL_ECAT_SM_OFS_STATUS] |= bit_mask;
}

static void sm_clear_status_bit(uint8_t *esc, unsigned sm_index, uint8_t bit_mask)
{
	uint16_t base = ul_ecat_sm_base(sm_index);
	esc[base + UL_ECAT_SM_OFS_STATUS] &= (uint8_t)~bit_mask;
}

int ul_ecat_slave_mailbox_write_sm1(ul_ecat_slave_t *s, const uint8_t *frame, size_t len)
{
	if (s == NULL || frame == NULL || len == 0u) {
		return -1;
	}
	ul_ecat_sm_config_t sm1;
	ul_ecat_sm_read(s->esc, 1, &sm1);
	if (!ul_ecat_sm_is_active(&sm1) || !ul_ecat_sm_is_mailbox(&sm1) || sm1.length == 0u) {
		return -1;
	}
	if (len > sm1.length) {
		return -1;
	}
	if ((size_t)sm1.start_addr + sm1.length > UL_ECAT_SLAVE_ESC_SIZE) {
		return -1;
	}
	uint8_t *dst = s->esc + sm1.start_addr;
	memcpy(dst, frame, len);
	if (len < sm1.length) {
		memset(dst + len, 0, sm1.length - len);
	}
	sm_set_status_bit(s->esc, 1, UL_ECAT_SM_STAT_MBX_FULL);
	return 0;
}

int ul_ecat_slave_mailbox_on_sm0_write(ul_ecat_slave_t *s, uint16_t write_ado, uint16_t write_len)
{
	if (s == NULL || write_len == 0u) {
		return 0;
	}
	ul_ecat_sm_config_t sm0;
	ul_ecat_sm_read(s->esc, 0, &sm0);
	if (!ul_ecat_sm_is_active(&sm0) || !ul_ecat_sm_is_mailbox(&sm0) || sm0.length == 0u) {
		return 0;
	}

	uint32_t sm0_end = (uint32_t)sm0.start_addr + sm0.length;
	uint32_t write_end = (uint32_t)write_ado + write_len;
	if (write_end != sm0_end) {
		return 0;
	}
	if (write_ado < sm0.start_addr) {
		return 0;
	}
	sm_set_status_bit(s->esc, 0, UL_ECAT_SM_STAT_MBX_FULL);
	return 1;
}

void ul_ecat_slave_mailbox_on_sm1_read(ul_ecat_slave_t *s, uint16_t read_ado, uint16_t read_len)
{
	if (s == NULL || read_len == 0u) {
		return;
	}
	ul_ecat_sm_config_t sm1;
	ul_ecat_sm_read(s->esc, 1, &sm1);
	if (!ul_ecat_sm_is_active(&sm1) || !ul_ecat_sm_is_mailbox(&sm1) || sm1.length == 0u) {
		return;
	}
	uint32_t sm1_end = (uint32_t)sm1.start_addr + sm1.length;
	uint32_t read_end = (uint32_t)read_ado + read_len;
	if (read_end != sm1_end) {
		return;
	}
	if (read_ado < sm1.start_addr) {
		return;
	}
	sm_clear_status_bit(s->esc, 1, UL_ECAT_SM_STAT_MBX_FULL);
}
