/**
 * @file osal_zephyr.c
 * @brief Zephyr OSAL: k_mutex, k_condvar, k_thread, monotonic time.
 */

#include "ul_ecat_osal.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define WORKER_STACK_SIZE 3072

static struct k_mutex g_trx_mutex;
static struct k_mutex g_q_mutex;
static struct k_mutex g_evt_mutex;
static struct k_condvar g_evt_cond;

static K_THREAD_STACK_DEFINE(g_worker_stack, WORKER_STACK_SIZE);
static struct k_thread g_worker_thread;
static k_tid_t g_worker_tid;

static ul_ecat_worker_fn_t g_worker_fn;
static void *g_worker_arg;

static K_SEM_DEFINE(g_worker_done, 0, 1);

static void worker_shim(void *p1, void *p2, void *p3)
{
	(void)p1;
	(void)p2;
	(void)p3;
	if (g_worker_fn != NULL) {
		g_worker_fn(g_worker_arg);
	}
	k_sem_give(&g_worker_done);
}

void ul_ecat_osal_init(void)
{
	k_mutex_init(&g_trx_mutex);
	k_mutex_init(&g_q_mutex);
	k_mutex_init(&g_evt_mutex);
	k_condvar_init(&g_evt_cond);
}

void ul_ecat_osal_shutdown(void)
{
}

void ul_ecat_osal_trx_lock(void)
{
	(void)k_mutex_lock(&g_trx_mutex, K_FOREVER);
}

void ul_ecat_osal_trx_unlock(void)
{
	k_mutex_unlock(&g_trx_mutex);
}

void ul_ecat_osal_q_lock(void)
{
	(void)k_mutex_lock(&g_q_mutex, K_FOREVER);
}

void ul_ecat_osal_q_unlock(void)
{
	k_mutex_unlock(&g_q_mutex);
}

void ul_ecat_osal_evt_lock(void)
{
	(void)k_mutex_lock(&g_evt_mutex, K_FOREVER);
}

void ul_ecat_osal_evt_unlock(void)
{
	k_mutex_unlock(&g_evt_mutex);
}

void ul_ecat_osal_evt_wait(void)
{
	(void)k_condvar_wait(&g_evt_cond, &g_evt_mutex, K_FOREVER);
}

void ul_ecat_osal_evt_signal(void)
{
	k_condvar_signal(&g_evt_cond);
}

uint64_t ul_ecat_osal_monotonic_ns(void)
{
	return k_ticks_to_ns_floor64(k_uptime_ticks());
}

void ul_ecat_osal_sleep_us(unsigned usec)
{
	k_usleep(usec);
}

void ul_ecat_osal_sleep_until_ns(uint64_t *abs_ns, uint64_t period_ns)
{
	uint64_t now = ul_ecat_osal_monotonic_ns();

	if ((int64_t)(*abs_ns - now) > 0) {
		uint64_t wait_ns = *abs_ns - now;

		while (wait_ns > 1000000ULL) {
			k_msleep(1);
			now = ul_ecat_osal_monotonic_ns();
			if ((int64_t)(*abs_ns - now) <= 0) {
				goto advance;
			}
			wait_ns = *abs_ns - now;
		}
		if (wait_ns > 0ULL) {
			k_usleep((uint32_t)((wait_ns + 999ULL) / 1000ULL));
		}
	}
advance:
	*abs_ns += period_ns;
}

void ul_ecat_osal_realtime_hint(int rt_priority)
{
	if (rt_priority > 0) {
		printk("ul_ecat: RT priority %d ignored on Zephyr (use thread priority in Kconfig).\n",
		       rt_priority);
	}
}

int ul_ecat_osal_worker_start(ul_ecat_worker_fn_t fn, void *arg, int rt_priority)
{
	(void)rt_priority;
	g_worker_fn = fn;
	g_worker_arg = arg;
	g_worker_tid = k_thread_create(&g_worker_thread, g_worker_stack,
				       K_THREAD_STACK_SIZEOF(g_worker_stack), worker_shim, NULL, NULL,
				       NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
	(void)g_worker_tid;
	return 0;
}

void ul_ecat_osal_worker_join(void)
{
	(void)k_sem_take(&g_worker_done, K_FOREVER);
}
