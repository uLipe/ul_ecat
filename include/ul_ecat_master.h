/**
 * @file ul_ecat_master.h
 * @brief Minimal EtherCAT master API (Linux AF_PACKET). Comments in English per project rules.
 *
 * Wire format follows ETG public EtherCAT frame/datagram description. SoE/FoE are not part
 * of this MVP; use a commercial stack or extend this library if you need those services.
 */

#ifndef UL_ECAT_MASTER_H
#define UL_ECAT_MASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * @brief Set destination MAC to Ethernet broadcast (common for EtherCAT L2 discovery).
 */
void ul_ecat_mac_broadcast(unsigned char dst[6]);

/**
 * @brief EtherCAT master settings for initialization.
 */
typedef struct {
    const char   *iface_name; /**< e.g. "eth0" */
    unsigned char dst_mac[6];
    unsigned char src_mac[6];
    int           rt_priority;    /**< SCHED_FIFO priority (1-99 typical); 0 = do not request RT */
    int           dc_enable;      /**< Whether to enable DC logic */
    uint32_t      dc_sync0_cycle; /**< Nominal cycle in ns (informational) */
} ul_ecat_master_settings_t;

typedef enum {
    UL_ECAT_SLAVE_STATE_INIT    = 1,
    UL_ECAT_SLAVE_STATE_PRE_OP  = 2,
    UL_ECAT_SLAVE_STATE_BOOT    = 3,
    UL_ECAT_SLAVE_STATE_SAFE_OP = 4,
    UL_ECAT_SLAVE_STATE_OP      = 8,
    UL_ECAT_SLAVE_STATE_ERROR   = 16
} ul_ecat_slave_state_t;

typedef struct {
    uint16_t station_address;
    ul_ecat_slave_state_t state;
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_no;
    uint32_t serial_no;
    char device_name[128];
} ul_ecat_slave_t;

#define UL_ECAT_MAX_SLAVES 16

typedef struct {
    ul_ecat_slave_t slaves[UL_ECAT_MAX_SLAVES];
    int slave_count;
} ul_ecat_master_slaves_t;

ul_ecat_master_slaves_t *ul_ecat_get_master_slaves(void);

/**
 * @brief Discover slaves on the attached network (assigns station addresses 0x1000+n).
 *
 * When the environment variable @c UL_ECAT_VERBOSE is set to a non-empty value other than @c 0,
 * the scan prints each APWR/FPRD datagram (useful for E2E demos).
 *
 * @return 0 on success, negative on error.
 */
int ul_ecat_scan_network(void);

/**
 * @brief Parse ESI XML files and attempt to match discovered slaves (requires prior scan).
 */
void ul_ecat_scan_slaves_with_esi(const char **file_list);

int ul_ecat_request_slave_state(int slave_index, ul_ecat_slave_state_t state);
int ul_ecat_request_slave_state_mailbox(int slave_index, ul_ecat_slave_state_t state);
int ul_ecat_poll_slave_state(int slave_index, ul_ecat_slave_state_t desired_state, int timeout_ms);

typedef enum {
    DC_EVENT_INIT = 0,
    DC_EVENT_SYNC,
    DC_EVENT_ERROR
} ul_ecat_dc_event_t;

typedef struct {
    ul_ecat_dc_event_t event;
    int64_t            offset_ns;
    int                error_code;
} ul_ecat_dc_event_info_t;

struct ul_ecat_frame {
    unsigned char dst_mac[6];
    unsigned char src_mac[6];
    unsigned short ethertype;
    unsigned char payload[1500];
};

int ul_ecat_master_init(const ul_ecat_master_settings_t *cfg);
int ul_ecat_master_shutdown(void);

/**
 * @brief Queue a Fixed Physical Read (FPRD): ADP = station address, ADO = ESC register offset.
 */
void ul_ecat_queue_fprd(uint16_t adp, uint16_t ado, uint16_t length);

/**
 * @brief Queue a Fixed Physical Write (FPWR).
 */
void ul_ecat_queue_fpwr(uint16_t adp, uint16_t ado, const void *data, uint16_t length);

typedef void (*ul_ecat_dc_callback_t)(const ul_ecat_dc_event_info_t *info);
typedef void (*ul_ecat_frame_callback_t)(const struct ul_ecat_frame *frame, ssize_t frame_len);

void ul_ecat_register_dc_callback(ul_ecat_dc_callback_t cb);
void ul_ecat_register_frame_callback(ul_ecat_frame_callback_t cb);
void ul_ecat_eventloop_run(void);
void ul_ecat_eventloop_stop(void);

void ul_ecat_send_batched_frames(void);
void ul_ecat_receive_frames_nonblock(void);

typedef struct {
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_no;
    uint32_t serial_no;
    char device_name[128];
} ul_ecat_esi_device_t;

typedef struct {
    ul_ecat_esi_device_t devices[16];
    int count;
} ul_ecat_esi_parse_result_t;

int ul_ecat_esi_parse_file(const char *filename, ul_ecat_esi_parse_result_t *out_result);

/**
 * @brief Embedded CLI helper used by the legacy C tool and tests.
 */
int ul_ecat_app_execute(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_MASTER_H */
