/**
 * @file ul_ecat_al.h
 * @brief EtherCAT Application Layer (AL) Control / Status register helpers (ETG.1000).
 *
 * AL Control @ 0x0120, AL Status @ 0x0130 (16-bit, little-endian on wire).
 */

#ifndef UL_ECAT_AL_H
#define UL_ECAT_AL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bits 0-3: requested state / status code (INIT=1, PREOP=2, BOOT=3, SAFEOP=4, OP=8). */
#define UL_ECAT_AL_MASK_STATE 0x000Fu

/** Bit 4 of AL Control: acknowledge (master acknowledges status change). */
#define UL_ECAT_AL_CTRL_ACK 0x0010u

/** Bit 4 of AL Status: error indication (device reported error). */
#define UL_ECAT_AL_STAT_ERR 0x0010u

/* AL Status Code values (ETG.1000 Table 11, subset). */
#define UL_ECAT_AL_ERR_NONE                    0x0000u
#define UL_ECAT_AL_ERR_UNSPECIFIED             0x0001u
#define UL_ECAT_AL_ERR_NO_MEMORY               0x0002u
#define UL_ECAT_AL_ERR_INVALID_STATE_CHANGE    0x0011u
#define UL_ECAT_AL_ERR_UNKNOWN_STATE           0x0012u
#define UL_ECAT_AL_ERR_BOOTSTRAP_NOT_SUPPORTED 0x0013u
#define UL_ECAT_AL_ERR_INVALID_MAILBOX_CFG     0x0016u
#define UL_ECAT_AL_ERR_INVALID_SM_CFG          0x0017u
#define UL_ECAT_AL_ERR_WATCHDOG_EXPIRED        0x001Bu

/**
 * Build 16-bit AL Control word for FPWR (little-endian payload).
 * @param state_nibble One of 1,2,3,4,8 for INIT..OP.
 * @param ack If non-zero, set acknowledge bit.
 */
uint16_t ul_ecat_al_control_word(uint8_t state_nibble, int ack);

/**
 * Extract AL state code from AL Status register value (lower nibble).
 */
uint8_t ul_ecat_al_status_state(uint16_t al_status);

/**
 * Non-zero if AL Status error indication bit is set.
 */
int ul_ecat_al_status_error_indicated(uint16_t al_status);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_AL_H */
