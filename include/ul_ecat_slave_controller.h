/**
 * @file ul_ecat_slave_controller.h
 * @brief Thin application-facing layer over the slave: software Ethernet path or LAN9252 SPI poll.
 *
 * - Software backend: delegates to @ref ul_ecat_slave_process_ethernet; use @ref ul_ecat_slave_controller_poll
 *   after processing frames to dispatch AL/status callbacks from the mirror.
 * - LAN9252 backend: @ref ul_ecat_slave_controller_poll reads the ESC via SPI into the mirror (registers &lt; 0x1000)
 *   and optional process-data segments; Ethernet replies are handled by the chip, not the MCU.
 */

#ifndef UL_ECAT_SLAVE_CONTROLLER_H
#define UL_ECAT_SLAVE_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "ul_ecat_slave.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ul_ecat_slave_backend {
    UL_ECAT_SLAVE_BACKEND_SOFTWARE_ETHERNET = 0,
    UL_ECAT_SLAVE_BACKEND_LAN9252_SPI = 1,
} ul_ecat_slave_backend_t;

typedef void (*ul_ecat_slave_on_al_status_t)(uint16_t al_status_word, void *user_ctx);

/** @p al_event is the 32-bit AL Event register (ETG @ 0x0220), little-endian in CPU memory. */
typedef void (*ul_ecat_slave_on_esc_event_t)(uint32_t al_event, void *user_ctx);

/**
 * Optional process-data slice (ETG addresses @p 0x1000 ..); not stored in @ref ul_ecat_slave_t::esc
 * (4 KiB mirror covers only register space). Set lengths to 0 to disable.
 */
typedef struct ul_ecat_slave_controller_pdram_cfg {
    uint16_t in_offset;
    uint16_t in_len;
    uint8_t *in_buf;
    uint16_t out_offset;
    uint16_t out_len;
    uint8_t *out_buf;
} ul_ecat_slave_controller_pdram_cfg_t;

typedef struct ul_ecat_slave_controller {
    ul_ecat_slave_t *slave;
    ul_ecat_slave_backend_t backend;
    void *user_ctx;
    uint16_t last_al_status;
    uint32_t last_al_event;
    ul_ecat_slave_on_al_status_t on_al_status;
    ul_ecat_slave_on_esc_event_t on_esc_event;
    ul_ecat_slave_controller_pdram_cfg_t pdram;
} ul_ecat_slave_controller_t;

/**
 * Initialize @p ctrl and the slave mirror (identity + default AL Status) via @ref ul_ecat_slave_init.
 * @param mac Ethernet SA used when building reply frames (software backend only).
 */
int ul_ecat_slave_controller_init(ul_ecat_slave_controller_t *ctrl,
                                  ul_ecat_slave_t *slave,
                                  ul_ecat_slave_backend_t backend,
                                  const uint8_t mac[6],
                                  const ul_ecat_slave_identity_t *id);

void ul_ecat_slave_controller_set_callbacks(ul_ecat_slave_controller_t *ctrl,
                                            ul_ecat_slave_on_al_status_t on_al_status,
                                            ul_ecat_slave_on_esc_event_t on_esc_event,
                                            void *user_ctx);

/**
 * Configure optional PDO buffers for the LAN9252 backend (ignored for software backend).
 * Call before @ref ul_ecat_slave_controller_poll. Safe to pass zero lengths to disable.
 */
void ul_ecat_slave_controller_set_pdram(ul_ecat_slave_controller_t *ctrl,
                                        const ul_ecat_slave_controller_pdram_cfg_t *cfg);

/**
 * Process one received Ethernet frame (software backend only). LAN9252 backend returns -ENOTSUP.
 */
int ul_ecat_slave_controller_process_ethernet(ul_ecat_slave_controller_t *ctrl,
                                            const uint8_t *rx, size_t rx_len,
                                            uint8_t *tx, size_t tx_cap, size_t *tx_len);

/**
 * Poll: software backend — scan mirror for AL Status / AL Event changes and invoke callbacks.
 * LAN9252 backend — SPI sync from chip into mirror (+ optional PD RAM in/out). @p timeout_ms reserved.
 */
int ul_ecat_slave_controller_poll(ul_ecat_slave_controller_t *ctrl, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_SLAVE_CONTROLLER_H */
