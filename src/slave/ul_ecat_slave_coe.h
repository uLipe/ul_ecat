/**
 * @file ul_ecat_slave_coe.h
 * @brief CoE / SDO server (slave). Internal header.
 *
 * Layout (over mailbox, ETG.1000 cap.5):
 *   [mbx_hdr 6B][coe_hdr 2B][sdo_hdr 8B][data...]
 *
 * coe_hdr (LE):
 *   bits  0..8  : number (0)
 *   bits  9..11 : reserved
 *   bits 12..15 : service (0x02 SDO Req, 0x03 SDO Resp)
 *
 * sdo_hdr Init Upload/Download Request (CiA 301):
 *   byte 0: command specifier (size, e/s/n/cmd bits)
 *   bytes 1..2: index (LE)
 *   byte 3: subindex
 *   bytes 4..7: data (expedited) or total length (segmented init)
 */

#ifndef UL_ECAT_SLAVE_COE_H
#define UL_ECAT_SLAVE_COE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UL_ECAT_COE_SVC_SDO_REQ      0x02u
#define UL_ECAT_COE_SVC_SDO_RESP     0x03u
#define UL_ECAT_COE_SVC_SDO_INFO     0x08u

/* SDO Info opcodes (ETG.1000 cap.5.6.3). */
#define UL_ECAT_SDO_INFO_OP_LIST_REQ   0x01u
#define UL_ECAT_SDO_INFO_OP_LIST_RESP  0x02u

/* SDO Info object list types. */
#define UL_ECAT_SDO_INFO_LIST_ALL     0x01u
#define UL_ECAT_SDO_INFO_LIST_RXPDO   0x02u
#define UL_ECAT_SDO_INFO_LIST_TXPDO   0x03u

/* SDO command-specifier upper bits (ccs/scs in CiA 301). */
#define UL_ECAT_SDO_CCS_DOWNLOAD_REQ_INIT  (1u << 5)
#define UL_ECAT_SDO_CCS_UPLOAD_REQ_INIT    (2u << 5)
#define UL_ECAT_SDO_CCS_UPLOAD_SEG_REQ     (3u << 5)
#define UL_ECAT_SDO_SCS_UPLOAD_SEG_RESP    (0u << 5)
#define UL_ECAT_SDO_SCS_UPLOAD_RESP_INIT   (2u << 5)
#define UL_ECAT_SDO_SCS_DOWNLOAD_RESP_INIT (3u << 5)
#define UL_ECAT_SDO_CMD_ABORT              (4u << 5)

/** Toggle bit (segmented requests/responses, alternating between 0 and 1). */
#define UL_ECAT_SDO_BIT_TOGGLE   0x10u
/** Continuation bit (segmented response): 1 = last segment, 0 = more to come. */
#define UL_ECAT_SDO_BIT_LAST_SEG 0x01u

#define UL_ECAT_SDO_BIT_EXPEDITED 0x02u
#define UL_ECAT_SDO_BIT_SIZE_IND  0x01u

/* Common SDO abort codes (CiA 301). */
#define UL_ECAT_SDO_ABORT_TOGGLE_BIT     0x05030000u
#define UL_ECAT_SDO_ABORT_ACCESS         0x06010000u
#define UL_ECAT_SDO_ABORT_WRITE_RO       0x06010002u
#define UL_ECAT_SDO_ABORT_READ_WO        0x06010001u
#define UL_ECAT_SDO_ABORT_OBJECT_GONE    0x06020000u
#define UL_ECAT_SDO_ABORT_BAD_SUBINDEX   0x06090011u
#define UL_ECAT_SDO_ABORT_INVALID_VALUE  0x06090030u
#define UL_ECAT_SDO_ABORT_LENGTH_MM      0x06070010u
#define UL_ECAT_SDO_ABORT_GENERAL        0x08000000u

/**
 * Process a complete mailbox frame (as delivered by the controller mailbox
 * dispatcher). If the frame is a CoE/SDO request, builds a CoE/SDO response
 * (or abort) and copies it into @p reply (capacity @p reply_cap).
 *
 * @return reply length on success (0..reply_cap), or 0 if the frame is not CoE
 *         (caller should ignore it).
 */
size_t ul_ecat_slave_coe_process(const uint8_t *frame, size_t len,
                                 uint8_t *reply, size_t reply_cap);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_SLAVE_COE_H */
