/**
 * @file osal_nuttx.c
 * @brief NuttX OSAL: pthread mutex/cond, worker thread, CLOCK_MONOTONIC, optional RT.
 *
 * NuttX provides POSIX pthreads and clocks; locked memory (mlockall) is omitted.
 */

#include "ul_ecat_osal.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t g_trx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_evt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_evt_cond = PTHREAD_COND_INITIALIZER;

static pthread_t g_worker_id;
static int g_worker_started;

void ul_ecat_osal_init(void)
{
}

void ul_ecat_osal_shutdown(void)
{
}

void ul_ecat_osal_trx_lock(void)
{
    pthread_mutex_lock(&g_trx_mutex);
}

void ul_ecat_osal_trx_unlock(void)
{
    pthread_mutex_unlock(&g_trx_mutex);
}

void ul_ecat_osal_q_lock(void)
{
    pthread_mutex_lock(&g_q_mutex);
}

void ul_ecat_osal_q_unlock(void)
{
    pthread_mutex_unlock(&g_q_mutex);
}

void ul_ecat_osal_evt_lock(void)
{
    pthread_mutex_lock(&g_evt_lock);
}

void ul_ecat_osal_evt_unlock(void)
{
    pthread_mutex_unlock(&g_evt_lock);
}

void ul_ecat_osal_evt_wait(void)
{
    pthread_cond_wait(&g_evt_cond, &g_evt_lock);
}

void ul_ecat_osal_evt_signal(void)
{
    pthread_cond_signal(&g_evt_cond);
}

uint64_t ul_ecat_osal_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void ul_ecat_osal_sleep_us(unsigned usec)
{
    usleep(usec);
}

void ul_ecat_osal_sleep_until_ns(uint64_t *abs_ns, uint64_t period_ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(*abs_ns / 1000000000ULL);
    ts.tv_nsec = (long)(*abs_ns % 1000000000ULL);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    *abs_ns += period_ns;
}

void ul_ecat_osal_realtime_hint(int rt_priority)
{
    if (rt_priority <= 0) {
        fprintf(stderr, "ul_ecat: RT priority 0 — not requesting SCHED_FIFO.\n");
        return;
    }
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = rt_priority;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        fprintf(stderr,
                "ul_ecat: warning: pthread_setschedparam(SCHED_FIFO) failed (%s).\n",
                strerror(errno));
    }
}

int ul_ecat_osal_worker_start(ul_ecat_worker_fn_t fn, void *arg, int rt_priority)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (rt_priority > 0) {
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = rt_priority;
        if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
            fprintf(stderr, "ul_ecat: warning: pthread_attr_setschedpolicy failed.\n");
        }
        if (pthread_attr_setschedparam(&attr, &sp) != 0) {
            fprintf(stderr, "ul_ecat: warning: pthread_attr_setschedparam failed.\n");
        }
    }
    int rc = pthread_create(&g_worker_id, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        fprintf(stderr,
                "ul_ecat: warning: pthread_create with SCHED_FIFO failed (%s); retrying without RT attrs.\n",
                strerror(rc));
        rc = pthread_create(&g_worker_id, NULL, fn, arg);
        if (rc != 0) {
            fprintf(stderr, "ul_ecat: pthread_create: %s\n", strerror(rc));
            return -1;
        }
    }
    g_worker_started = 1;
    return 0;
}

void ul_ecat_osal_worker_join(void)
{
    if (g_worker_started) {
        pthread_join(g_worker_id, NULL);
        g_worker_started = 0;
    }
}
