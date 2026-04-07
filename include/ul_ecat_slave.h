/**
 * @file ul_ecat_slave.h
 * @brief Minimal EtherCAT slave (ESC register model + PDU reply) for host simulation and devices.
 */

#ifndef UL_ECAT_SLAVE_H
#define UL_ECAT_SLAVE_H

#include <stddef.h>
#include <stdint.h>

#include "ul_ecat_esc_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ESC RAM size (byte-addressable mirror). */
#define UL_ECAT_SLAVE_ESC_SIZE 4096u

/**
 * Identity values used to fill vendor/product/revision/serial registers (little-endian in RAM).
 */
typedef struct ul_ecat_slave_identity {
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision;
    uint32_t serial;
} ul_ecat_slave_identity_t;

typedef struct ul_ecat_slave {
    uint8_t esc[UL_ECAT_SLAVE_ESC_SIZE];
    uint8_t mac[6];
    /** Auto-increment ring position for APWR (usually 0 for first slave). */
    uint16_t logical_position;
    uint8_t reserved[2];
} ul_ecat_slave_t;

/**
 * Initialize slave: clear ESC, set @p id into vendor/product/rev/serial registers,
 * set AL Status to INIT (0x1 in state nibble), station 0.
 * @p mac is our Ethernet source address in frames we emit.
 */
void ul_ecat_slave_init(ul_ecat_slave_t *s, const uint8_t mac[6], const ul_ecat_slave_identity_t *id);

void ul_ecat_slave_reset(ul_ecat_slave_t *s, const ul_ecat_slave_identity_t *id);

/**
 * Process raw EtherCAT datagram area (PDU body after 4-byte ECAT header in spec terms:
 * here @p pdu_in is the concatenation of datagrams only, same layout as master uses for EC payload).
 */
int ul_ecat_slave_process_pdu(ul_ecat_slave_t *s,
                              const uint8_t *pdu_in, size_t pdu_in_len,
                              uint8_t *pdu_out, size_t pdu_out_cap, size_t *pdu_out_len);

/**
 * Process one received Ethernet frame and build a reply frame (EtherCAT response).
 * Reply DA = incoming SA (master), reply SA = @p s->mac.
 * @param tx_cap must hold at least one full MTU-sized frame buffer.
 * @returns 0 on success, negative on error; @p tx_len set to reply length (0 if no reply).
 */
int ul_ecat_slave_process_ethernet(ul_ecat_slave_t *s,
                                   const uint8_t *rx, size_t rx_len,
                                   uint8_t *tx, size_t tx_cap, size_t *tx_len);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_SLAVE_H */
