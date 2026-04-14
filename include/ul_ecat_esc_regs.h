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
#define UL_ECAT_ESC_REG_ALSTAT    0x0130u
/** AL Status Code (2 bytes, ETG.1000 Table 11). */
#define UL_ECAT_ESC_REG_ALSTACODE 0x0134u
/** AL Event register (4 bytes, ETG). */
#define UL_ECAT_ESC_REG_ALEVENT   0x0220u

/** DC System Time (8 bytes, read from first slave). */
#define UL_ECAT_ESC_REG_DCSYS0  0x0910u
/** DC System Time Offset (4-byte signed correction written by master). */
#define UL_ECAT_ESC_REG_DCSYSOFS 0x0920u

#endif /* UL_ECAT_ESC_REGS_H */
