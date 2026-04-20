/**
 * @file ul_ecat_slave_mailbox.h
 * @brief Slave-side mailbox transport (ETG.1000 cap.5). Internal header.
 *
 * Mailbox frames are carried over SM0 (master->slave) and SM1 (slave->master),
 * both configured in mailbox mode with handshake via SM Status bits.
 *
 * Header layout (6 bytes, little-endian):
 *   [0..1] length        (body length, excluding header)
 *   [2..3] address       (station address)
 *   [4]    channel+prio  (bits [5:0]=channel, [7:6]=priority)
 *   [5]    type+counter  (bits [3:0]=type, [7:4]=counter)
 */

#ifndef UL_ECAT_SLAVE_MAILBOX_H
#define UL_ECAT_SLAVE_MAILBOX_H

#include "ul_ecat_slave.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint16_t length;
	uint16_t address;
	uint8_t  channel;
	uint8_t  priority;
	uint8_t  type;
	uint8_t  counter;
} ul_ecat_mbx_header_t;

/**
 * Callback invoked when a complete mailbox frame lands in SM0.
 * @p frame points to the full SM0 buffer including header; @p len is the SM0 length.
 * @p user_ctx is the context registered via ul_ecat_slave_controller_set_mailbox_handler.
 *
 * Handler may call ul_ecat_slave_mailbox_reply() to enqueue a response into SM1.
 */
typedef void (*ul_ecat_slave_mailbox_rx_t)(const uint8_t *frame, size_t len, void *user_ctx);

/**
 * Decode the 6-byte header at @p frame (which must have at least UL_ECAT_MBX_HDR_LEN bytes).
 */
void ul_ecat_mbx_header_decode(const uint8_t *frame, ul_ecat_mbx_header_t *out);

/**
 * Encode header into the first 6 bytes of @p frame.
 */
void ul_ecat_mbx_header_encode(uint8_t *frame, const ul_ecat_mbx_header_t *hdr);

/**
 * Write a mailbox reply into SM1's buffer in the ESC mirror and set mailbox-full flag
 * in SM1 Status. @p frame is copied verbatim; its length must fit SM1.length.
 *
 * @return 0 on success, -1 on invalid SM1 config or oversized frame.
 */
int ul_ecat_slave_mailbox_write_sm1(ul_ecat_slave_t *s, const uint8_t *frame, size_t len);

/**
 * Check whether the full SM0 range has just been written (mailbox request complete).
 * Called by PDU processing after an FPWR whose range overlaps SM0.
 *
 * If the write ends exactly at SM0.start+SM0.length, sets SM0 Status MBX_FULL bit
 * and returns 1 (caller should dispatch the handler). Otherwise returns 0.
 */
int ul_ecat_slave_mailbox_on_sm0_write(ul_ecat_slave_t *s, uint16_t write_ado, uint16_t write_len);

/**
 * Called by PDU processing after an FPRD on SM1 completes: if the full SM1 range
 * was read, clears SM1 Status MBX_FULL bit.
 */
void ul_ecat_slave_mailbox_on_sm1_read(ul_ecat_slave_t *s, uint16_t read_ado, uint16_t read_len);

#endif /* UL_ECAT_SLAVE_MAILBOX_H */
