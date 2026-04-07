/**
 * @file ul_ecat_esc_regs.h
 * @brief Common ESC register offsets (byte-addressed, 16-bit access typical).
 *
 * Shared between master and slave stacks; values follow public ETG register map summaries.
 */

#ifndef UL_ECAT_ESC_REGS_H
#define UL_ECAT_ESC_REGS_H

#include <stdint.h>

#define UL_ECAT_ESC_REG_STADR   0x0010u
#define UL_ECAT_ESC_REG_VENDOR  0x0012u
#define UL_ECAT_ESC_REG_PRODUCT 0x0016u
#define UL_ECAT_ESC_REG_REV     0x001Au
#define UL_ECAT_ESC_REG_SERIAL  0x001Eu
#define UL_ECAT_ESC_REG_ALCTL   0x0120u
#define UL_ECAT_ESC_REG_ALSTAT  0x0130u

#endif /* UL_ECAT_ESC_REGS_H */
