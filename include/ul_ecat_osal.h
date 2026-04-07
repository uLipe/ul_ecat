/**
 * @file ul_ecat_osal.h
 * @brief OS abstraction: mutexes, worker thread, monotonic time, sleep (Linux vs Zephyr).
 */

#ifndef UL_ECAT_OSAL_H
#define UL_ECAT_OSAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ul_ecat_osal_init(void);

void ul_ecat_osal_shutdown(void);

void ul_ecat_osal_trx_lock(void);
void ul_ecat_osal_trx_unlock(void);

void ul_ecat_osal_q_lock(void);
void ul_ecat_osal_q_unlock(void);

void ul_ecat_osal_evt_lock(void);
void ul_ecat_osal_evt_unlock(void);

/** Caller must hold ul_ecat_osal_evt_lock. */
void ul_ecat_osal_evt_wait(void);

/** Caller must hold ul_ecat_osal_evt_lock. */
void ul_ecat_osal_evt_signal(void);

typedef void *(*ul_ecat_worker_fn_t)(void *arg);

/**
 * Start periodic worker thread. Join in ul_ecat_osal_worker_join().
 */
int ul_ecat_osal_worker_start(ul_ecat_worker_fn_t fn, void *arg, int rt_priority);

void ul_ecat_osal_worker_join(void);

uint64_t ul_ecat_osal_monotonic_ns(void);

void ul_ecat_osal_sleep_us(unsigned usec);

/**
 * Absolute monotonic sleep until *abs_ns (updates *abs_ns by period_ns on return for periodic loops).
 */
void ul_ecat_osal_sleep_until_ns(uint64_t *abs_ns, uint64_t period_ns);

/**
 * Linux: mlockall + SCHED_FIFO hints. Zephyr: no-op or log.
 */
void ul_ecat_osal_realtime_hint(int rt_priority);

#ifdef __cplusplus
}
#endif

#endif /* UL_ECAT_OSAL_H */
