#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

# Create Linux-specific fp_config_user.c
# Usage: ./create_fp_config_user.sh <output_dir>

OUTPUT_DIR="${1:-.}"

cat > "$OUTPUT_DIR/fp_config_user.c" << 'EOF'
/*
 * Linux POSIX pthreads-based FP configuration
 * This is a minimal implementation for testing on Linux
 */

#include "fp_config_user.h"
#include "fp_disp_session_mgr.h"
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* Mutex for registry locking */
static pthread_mutex_t fp_registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* Mutexes for section locking */
static pthread_mutex_t fp_section_locks[FP_MAX_SECTION_LOCKS_CNT];

/* Event objects - one per potential concurrent request */
static struct {
    pthread_cond_t cond;
    pthread_mutex_t lock;
    int signaled;
} fp_events[4];

/* Timer tracking */
static struct timespec fp_timers[4];

/* Minimal send function */
static bool fp_send_csp(void *p_disp_ctx)
{
    /* This is a placeholder - actual sending is handled by fp_csp_binder */
    return true;
}

/* System context */
static fp_dispatch_sys_context_t fp_sys_contexts[FP_CONFIG_SYS_CTX_TYPE_MAX] = {
    [FP_CONFIG_SYS_CTX_TYPE_LOCAL] = {
        .p_section_locks = fp_section_locks,
        .p_send = &fp_send_csp
    },
    [FP_CONFIG_SYS_CTX_TYPE_CSP] = {
        .p_section_locks = fp_section_locks,
        .p_send = &fp_send_csp
    },
};

/* Initialize section locks */
void fp_config_user_init(void)
{
    /* Use recursive mutexes so that fp_disp_session_mgr_track_sessions (which holds
     * a section lock) can call back into fp_disp_session_mgr_find/close_session from
     * within the user p_track callback without deadlocking.  The CMSIS template uses
     * osMutexRecursive for the same reason. */
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
    for (int i = 0; i < FP_MAX_SECTION_LOCKS_CNT; i++) {
        pthread_mutex_init(&fp_section_locks[i], &mattr);
    }
    pthread_mutexattr_destroy(&mattr);
    for (int i = 0; i < 4; i++) {
        pthread_cond_init(&fp_events[i].cond, NULL);
        pthread_mutex_init(&fp_events[i].lock, NULL);
        fp_events[i].signaled = 0;
    }
}

/* Assign system context */
void fp_config_user_assign_sys_context(fp_dispatch_info_t *p_disp_ctx, fp_config_sys_ctx_type_t sys_ctx_type)
{
    if (p_disp_ctx && sys_ctx_type < FP_CONFIG_SYS_CTX_TYPE_MAX) {
        p_disp_ctx->p_sys_ctx = &fp_sys_contexts[sys_ctx_type];
    }
}

/* Event synchronization */
void fp_arch_init_event(void **pp_event)
{
    if (pp_event) {
        static int event_idx = 0;
        if (event_idx >= 4) event_idx = 0;
        *pp_event = &fp_events[event_idx];
        fp_events[event_idx].signaled = 0;
        event_idx++;
    }
}

bool fp_arch_wait_for_event(void *p_event, const uint32_t timeout_ms)
{
    if (!p_event) return false;
    
    struct {
        pthread_cond_t cond;
        pthread_mutex_t lock;
        int signaled;
    } *event = (typeof(event))p_event;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    pthread_mutex_lock(&event->lock);
    bool result = true;
    while (!event->signaled) {
        int ret = pthread_cond_timedwait(&event->cond, &event->lock, &ts);
        if (ret == ETIMEDOUT) {
            result = false;
            break;
        }
    }
    event->signaled = 0;
    pthread_mutex_unlock(&event->lock);
    return result;
}

void fp_arch_signal_event(void *p_event)
{
    if (!p_event) return;
    
    struct {
        pthread_cond_t cond;
        pthread_mutex_t lock;
        int signaled;
    } *event = (typeof(event))p_event;
    
    pthread_mutex_lock(&event->lock);
    event->signaled = 1;
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->lock);
}

/* Registry locking */
bool fp_arch_lock_registry(void)
{
    return pthread_mutex_lock(&fp_registry_lock) == 0;
}

void fp_arch_unlock_registry(void)
{
    pthread_mutex_unlock(&fp_registry_lock);
}

/* Section locking */
bool fp_arch_lock_section(void *const p_lock)
{
    if (!p_lock) return false;
    return pthread_mutex_lock((pthread_mutex_t *)p_lock) == 0;
}

void fp_arch_unlock_section(void *const p_lock)
{
    if (p_lock) {
        pthread_mutex_unlock((pthread_mutex_t *)p_lock);
    }
}

/* Timer management */
void fp_arch_start_timer(fp_arch_timer_handle_t *const p_timer_hnd)
{
    if (p_timer_hnd && *p_timer_hnd < 4) {
        clock_gettime(CLOCK_REALTIME, &fp_timers[*p_timer_hnd]);
    }
}

void fp_arch_stop_timer(fp_arch_timer_handle_t *const p_timer_hnd)
{
    if (p_timer_hnd && *p_timer_hnd < 4) {
        memset(&fp_timers[*p_timer_hnd], 0, sizeof(struct timespec));
    }
}

bool fp_arch_is_timer_elapsed(fp_arch_timer_handle_t *const p_timer_hnd, const uint32_t elapse_time)
{
    if (!p_timer_hnd || *p_timer_hnd >= 4) return true;
    
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    
    uint32_t elapsed_ms = (now.tv_sec - fp_timers[*p_timer_hnd].tv_sec) * 1000 +
                          (now.tv_nsec - fp_timers[*p_timer_hnd].tv_nsec) / 1000000;
    return elapsed_ms >= elapse_time;
}

/* Hash calculation - simple CRC16 */
uint16_t fp_arch_calc_hash(const uint8_t *const p_data, const uint32_t data_sz)
{
    if (!p_data) return 0;
    
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < data_sz; i++) {
        crc ^= p_data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* Assertions */
/* Note: condition is !cond (abort on FALSE). The original generated code had
 * the inverted form "if (cond)" which fired on success — fixed in c4a93b4. */
void fp_arch_assert(const bool cond)
{
    if (!cond) {
        fprintf(stderr, "FP assertion failed\n");
        exit(1);
    }
}

void fp_arch_break_assert(const bool cond)
{
    if (!cond) {
        fprintf(stderr, "FP break assertion failed\n");
    }
}
EOF

