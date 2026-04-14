/**
 * @file ul_ecat_slave_al.h
 * @brief AL state machine for the slave ESC mirror (internal header).
 *
 * Called from PDU processing when the master writes to AL Control (0x0120).
 * Updates AL Status (0x0130) and AL Status Code (0x0134) in the ESC mirror.
 */

#ifndef UL_ECAT_SLAVE_AL_H
#define UL_ECAT_SLAVE_AL_H

#include "ul_ecat_slave.h"

/**
 * Process an AL Control write that just landed in the ESC mirror.
 *
 * Reads ALCTL, validates the requested transition against the current ALSTAT,
 * and writes the result (new state or error) back into ALSTAT + ALSTACODE.
 */
void ul_ecat_slave_al_on_control_write(ul_ecat_slave_t *s);

#endif
