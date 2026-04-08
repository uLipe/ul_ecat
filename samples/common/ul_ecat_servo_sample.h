/**
 * @file ul_ecat_servo_sample.h
 * @brief Fictitious CiA402-style PDO layout + identity for RTOS servo samples (no CoE stack).
 *
 * PDO offsets are application conventions; the real EtherCAT mapping comes from EEPROM/ESI
 * on hardware. These match the buffers passed to @ref ul_ecat_slave_controller_set_pdram.
 */

#ifndef UL_ECAT_SERVO_SAMPLE_H
#define UL_ECAT_SERVO_SAMPLE_H

#include <stdint.h>

#include "ul_ecat_slave.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ETG process data: master -> slave (RxPDO into the slave application). */
#define UL_ECAT_SERVO_RXPDO_OFF 0x1000u
/** ETG process data: slave -> master (TxPDO from the slave application). */
#define UL_ECAT_SERVO_TXPDO_OFF 0x1100u

#define UL_ECAT_SERVO_PDO_BYTES 12u

/** Fictitious vendor / product for demos (not a registered ETG identity). */
#define UL_ECAT_SERVO_VENDOR_ID 0xEC407001u
#define UL_ECAT_SERVO_PRODUCT_CODE 0x53565230u /* "SVR0" */
#define UL_ECAT_SERVO_REVISION 0x00010000u
#define UL_ECAT_SERVO_SERIAL 0x00000001u

static inline void ul_ecat_servo_sample_identity(ul_ecat_slave_identity_t *out)
{
	if (out == NULL) {
		return;
	}
	out->vendor_id = UL_ECAT_SERVO_VENDOR_ID;
	out->product_code = UL_ECAT_SERVO_PRODUCT_CODE;
	out->revision = UL_ECAT_SERVO_REVISION;
	out->serial = UL_ECAT_SERVO_SERIAL;
}

/**
 * Packed PDO (little-endian) — int32 position, velocity, torque (fixed-point torque in mNm optional).
 */
typedef struct ul_ecat_servo_rxpdo {
	int32_t target_position;
	int32_t target_velocity;
	int32_t target_torque;
} ul_ecat_servo_rxpdo_t;

typedef struct ul_ecat_servo_txpdo {
	int32_t actual_position;
	int32_t actual_velocity;
	int32_t actual_torque;
} ul_ecat_servo_txpdo_t;

#ifdef __cplusplus
}
#endif

#endif
