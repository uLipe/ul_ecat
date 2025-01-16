/**
 * @file ul_ecat_frame.h
 * @brief EtherCAT PDU/datagram encode and decode (ETG wire layout).
 *
 * Ethernet: DA(6) SA(6) EtherType 0x88A4 | EtherCAT PDU.
 * EtherCAT PDU: length(2) reserved(2) | datagrams...
 * Each datagram: header(10) + data(len) + WKC(2).
 */

#ifndef UL_ECAT_FRAME_H
#define UL_ECAT_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UL_ECAT_ETHERTYPE 0x88A4u

/** EtherCAT datagram commands (subset used by this stack). */
enum ul_ecat_dgram_cmd {
    UL_ECAT_CMD_APRD = 0x01,
    UL_ECAT_CMD_APWR = 0x02,
    UL_ECAT_CMD_FPRD = 0x04,
    UL_ECAT_CMD_FPWR = 0x05,
    UL_ECAT_CMD_BRD  = 0x07,
    UL_ECAT_CMD_BWR  = 0x08,
    UL_ECAT_CMD_LRD  = 0x0A,
    UL_ECAT_CMD_LWR  = 0x0B,
    UL_ECAT_CMD_LRW  = 0x0C,
};

/** Fixed 10-byte datagram header (before data, before WKC). */
#define UL_ECAT_DGRAM_HDR_LEN 10u
#define UL_ECAT_DGRAM_WKC_LEN 2u

/** EtherCAT PDU header after Ethernet header (length + reserved). */
#define UL_ECAT_PDU_HDR_LEN 4u

/** Max data bytes per datagram (11-bit length field in spec). */
#define UL_ECAT_DGRAM_MAX_DATA 1498u

/** Max EtherCAT payload (datagrams) inside one Ethernet frame (practical cap). */
#define UL_ECAT_MAX_EC_PAYLOAD 1498u

/**
 * Pack the 16-bit length/IRQ field: bits 0-10 = data length in bytes, bits 11-15 = IRQ.
 */
uint16_t ul_ecat_pack_len_irq(uint16_t data_len_bytes, uint16_t irq);

/**
 * Unpack data length from length/IRQ field.
 */
uint16_t ul_ecat_unpack_len(uint16_t len_irq);

/**
 * Size of one datagram on the wire: 10 + data_len + 2 (WKC).
 */
size_t ul_ecat_dgram_wire_size(uint16_t data_len_bytes);

/**
 * Encode a single datagram at @p out (must hold ul_ecat_dgram_wire_size(data_len)).
 * @param wkc_out Initial working counter value for TX (usually 0).
 */
int ul_ecat_dgram_encode(uint8_t *out, size_t out_cap,
                         uint8_t cmd, uint8_t index,
                         uint16_t adp, uint16_t ado,
                         uint16_t data_len_bytes, uint16_t irq, uint16_t wkc_out,
                         const void *data);

/**
 * Parse EtherCAT PDU starting at @p pdu after the 4-byte ECAT header.
 * @param pdu_len Total length of datagram area (sum of all dgram sizes).
 * @param dgram_index 0-based datagram to extract.
 * @param data_out Optional buffer for datagram data (may be NULL to skip copy).
 * @param data_cap Capacity of data_out.
 * @returns 0 on success, negative on error.
 */
int ul_ecat_dgram_parse(const uint8_t *pdu, size_t pdu_len, unsigned dgram_index,
                        uint8_t *cmd_out, uint8_t *index_out,
                        uint16_t *adp_out, uint16_t *ado_out,
                        uint16_t *data_len_out, uint16_t *irq_out,
                        uint16_t *wkc_out,
                        void *data_out, size_t data_cap);

/**
 * Count datagrams in PDU blob (walks until end).
 */
int ul_ecat_pdu_count_datagrams(const uint8_t *pdu, size_t pdu_len);

/**
 * Build full Ethernet frame: DA(6)+SA(6)+type(2)+ECAT length(2)+reserved(2)+datagrams.
 * @param ec_payload Buffer holding concatenated encoded datagrams.
 * @param ec_payload_len Sum of encoded datagram sizes.
 * @param frame_out Output buffer.
 * @param frame_cap Capacity of frame_out.
 * @returns frame length on success, negative on error.
 */
ssize_t ul_ecat_build_eth_frame(const uint8_t dst[6], const uint8_t src[6],
                                const uint8_t *ec_payload, size_t ec_payload_len,
                                uint8_t *frame_out, size_t frame_cap);

/**
 * Parse received Ethernet frame: validates EtherType and copies ECAT payload.
 * @param ec_payload_out Receives pointer into @p frame (no alloc) to PDU datagram area.
 * @param ec_payload_len_out Length of datagram area.
 * @param frame_len Total bytes received (Ethernet frame length).
 */
int ul_ecat_parse_eth_frame(const uint8_t *frame, size_t frame_len,
                            const uint8_t **ec_payload_out, size_t *ec_payload_len_out);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_FRAME_H */
