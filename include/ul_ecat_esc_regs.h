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

/* SyncManager registers (8 bytes each, ETG.1000 Table 23). */
#define UL_ECAT_ESC_REG_SM0     0x0800u
#define UL_ECAT_ESC_REG_SM1     0x0808u
#define UL_ECAT_ESC_REG_SM2     0x0810u
#define UL_ECAT_ESC_REG_SM3     0x0818u
#define UL_ECAT_ESC_SM_COUNT    4u
#define UL_ECAT_ESC_SM_SIZE     8u

/* Offsets within each SM register block. */
#define UL_ECAT_SM_OFS_START_ADDR  0u  /* 2 bytes: physical start address */
#define UL_ECAT_SM_OFS_LENGTH      2u  /* 2 bytes: buffer length */
#define UL_ECAT_SM_OFS_CONTROL     4u  /* 1 byte: mode + direction */
#define UL_ECAT_SM_OFS_STATUS      5u  /* 1 byte */
#define UL_ECAT_SM_OFS_ACTIVATE    6u  /* 1 byte: bit 0 = enable */
#define UL_ECAT_SM_OFS_PDI_CTRL    7u  /* 1 byte */

/* SM Control register bits. */
#define UL_ECAT_SM_CTRL_MODE_MASK  0x03u
#define UL_ECAT_SM_CTRL_MODE_BUF   0x00u  /* 3-buffer (process data) */
#define UL_ECAT_SM_CTRL_MODE_MBX   0x02u  /* mailbox (handshake) */
#define UL_ECAT_SM_CTRL_DIR_MASK   0x04u  /* bit 2: 0=read (slave->master), 1=write (master->slave) */

/* SM Status register bits (offset +5 within SM block). */
#define UL_ECAT_SM_STAT_INT_WRITE  0x01u  /* interrupt on buffer write */
#define UL_ECAT_SM_STAT_INT_READ   0x02u  /* interrupt on buffer read */
#define UL_ECAT_SM_STAT_MBX_FULL   0x08u  /* mailbox: buffer full (data pending) */

/* Mailbox protocol types (ETG.1000 cap.5 Table 29). */
#define UL_ECAT_MBX_TYPE_ERROR 0x00u
#define UL_ECAT_MBX_TYPE_AOE   0x01u  /* ADS over EtherCAT */
#define UL_ECAT_MBX_TYPE_EOE   0x02u  /* Ethernet over EtherCAT */
#define UL_ECAT_MBX_TYPE_COE   0x03u  /* CANopen over EtherCAT */
#define UL_ECAT_MBX_TYPE_FOE   0x04u  /* File over EtherCAT */
#define UL_ECAT_MBX_TYPE_SOE   0x05u  /* Servo drive over EtherCAT */
#define UL_ECAT_MBX_TYPE_VOE   0x0Fu  /* Vendor specific */

/** Mailbox header size (ETG.1000 cap.5 Table 30): length(2) + address(2) + channel/prio(1) + type/cnt(1). */
#define UL_ECAT_MBX_HDR_LEN 6u

/** DC System Time (8 bytes, read from first slave). */
#define UL_ECAT_ESC_REG_DCSYS0  0x0910u
/** DC System Time Offset (4-byte signed correction written by master). */
#define UL_ECAT_ESC_REG_DCSYSOFS 0x0920u

#endif /* UL_ECAT_ESC_REGS_H */
