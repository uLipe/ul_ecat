/**
 * @file ul_ecat_slave_al.c
 * @brief Slave-side AL state machine (ETG.1000 cap.6).
 */

#include "ul_ecat_slave_al.h"

#include "ul_ecat_al.h"
#include "ul_ecat_esc.h"
#include "ul_ecat_esc_regs.h"
#include "ul_ecat_slave_sm.h"

#include <string.h>

#define AL_INIT   1u
#define AL_PREOP  2u
#define AL_BOOT   3u
#define AL_SAFEOP 4u
#define AL_OP     8u

static int is_valid_transition(uint8_t from, uint8_t to)
{
	if (to == AL_INIT) {
		return 1;
	}

	switch (from) {
	case AL_INIT:   return to == AL_PREOP;
	case AL_PREOP:  return to == AL_SAFEOP;
	case AL_SAFEOP: return to == AL_OP || to == AL_PREOP;
	case AL_OP:     return to == AL_SAFEOP;
	default:
		break;
	}
	return 0;
}

static void write_al_status(uint8_t *esc, uint16_t status, uint16_t code)
{
	esc[UL_ECAT_ESC_REG_ALSTAT]     = (uint8_t)(status & 0xFFu);
	esc[UL_ECAT_ESC_REG_ALSTAT + 1] = (uint8_t)((status >> 8) & 0xFFu);
	esc[UL_ECAT_ESC_REG_ALSTACODE]     = (uint8_t)(code & 0xFFu);
	esc[UL_ECAT_ESC_REG_ALSTACODE + 1] = (uint8_t)((code >> 8) & 0xFFu);
}

void ul_ecat_slave_al_on_control_write(ul_ecat_slave_t *s)
{
	if (s == NULL) {
		return;
	}

	uint16_t alctl = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_ALCTL);
	uint16_t alstat = ul_ecat_esc_read_u16_le(s->esc, UL_ECAT_ESC_REG_ALSTAT);

	uint8_t req = (uint8_t)(alctl & UL_ECAT_AL_MASK_STATE);
	uint8_t cur = ul_ecat_al_status_state(alstat);

	if (alctl & UL_ECAT_AL_CTRL_ACK) {
		/* Master acknowledges an error; clear error indicator if states match. */
		if (req == cur) {
			write_al_status(s->esc, (uint16_t)cur, UL_ECAT_AL_ERR_NONE);
		}
		return;
	}

	if (req == cur) {
		return;
	}

	if (req == AL_BOOT) {
		write_al_status(s->esc,
				(uint16_t)(cur | UL_ECAT_AL_STAT_ERR),
				UL_ECAT_AL_ERR_BOOTSTRAP_NOT_SUPPORTED);
		return;
	}

	if (!is_valid_transition(cur, req)) {
		write_al_status(s->esc,
				(uint16_t)(cur | UL_ECAT_AL_STAT_ERR),
				UL_ECAT_AL_ERR_INVALID_STATE_CHANGE);
		return;
	}

	/* INIT->PREOP requires mailbox SyncManagers (SM0+SM1) to be configured. */
	if (cur == AL_INIT && req == AL_PREOP) {
		if (ul_ecat_sm_validate_mailbox_pair(s->esc) != 0) {
			write_al_status(s->esc,
					(uint16_t)(cur | UL_ECAT_AL_STAT_ERR),
					UL_ECAT_AL_ERR_INVALID_MAILBOX_CFG);
			return;
		}
	}

	write_al_status(s->esc, (uint16_t)req, UL_ECAT_AL_ERR_NONE);
}
