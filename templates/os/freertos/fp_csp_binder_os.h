/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_OS_H
#define FP_CSP_BINDER_OS_H

/**
 * @file fp_csp_binder_os.h  (FreeRTOS port)
 * @brief OS port header for fp_csp_binder targeting FreeRTOS.
 *
 * This file is the FreeRTOS-specific implementation of the OS abstraction required by
 * fp_csp_binder_port.h. Copy it into your project's include path under the name
 * "fp_csp_binder_os.h" (or adjust the include path so it is found before any other
 * file of the same name).
 *
 * Responsibilities:
 *   1. Provide the ES_TRACE_* logging macros (see PLATFORM LOGGING below).
 *   2. Define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS with the FreeRTOS-specific thread fields.
 *
 * The assertion macros BREAK_ASSERT and CRIT_ASSERT are defined in fp_csp_binder_cfg.h.
 */

#include <stdbool.h>
#include <stdint.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"

/*
 ***************************************************************************************************
 * PLATFORM LOGGING
 * The core logs through ES_TRACE_ERROR/WARN/INFO/DEBUG(fmt, ...) with printf-style arguments.
 * Map them to your platform's logger here. The defaults below discard all trace output.
 ***************************************************************************************************
 */
/** @brief Discards a trace message; its arguments still count as used, so no unused-variable warnings. */
static inline void fp_csp_binder_trace_discard(const char *fmt, ...) { (void)fmt; }

#ifndef ES_TRACE_ERROR
#define ES_TRACE_ERROR(...) fp_csp_binder_trace_discard(__VA_ARGS__)
#endif
#ifndef ES_TRACE_WARN
#define ES_TRACE_WARN(...)  fp_csp_binder_trace_discard(__VA_ARGS__)
#endif
#ifndef ES_TRACE_INFO
#define ES_TRACE_INFO(...)  fp_csp_binder_trace_discard(__VA_ARGS__)
#endif
#ifndef ES_TRACE_DEBUG
#define ES_TRACE_DEBUG(...) fp_csp_binder_trace_discard(__VA_ARGS__)
#endif

/*
 ***************************************************************************************************
 * OS-SPECIFIC STRUCT FIELDS
 * Injected at the end of csp_server_cfg_t by fp_csp_binder_port.h.
 ***************************************************************************************************
 */

/**
 * @brief FreeRTOS thread fields appended to csp_server_cfg_t.
 *
 * - p_thread_hnd:        FreeRTOS task handle returned by xTaskCreateStatic().
 * - thread_tcb:          Statically allocated task control block (StaticTask_t).
 * - thread_stack_buf:    Statically allocated stack memory for the task. The size is
 *                        determined by SERVER_THREAD_STACK_SZ from fp_csp_binder_cfg.h.
 *                        NOTE: In FreeRTOS, xTaskCreateStatic() takes stack size in
 *                        *words* (StackType_t elements), not bytes. Ensure SERVER_THREAD_STACK_SZ
 *                        is defined in words, or scale appropriately in fp_csp_binder_os.c.
 * - p_thread_func:       Pointer to the server thread entry function (fp_csp_binder_rx_thread_func).
 * - thread_stack_size:   Stack size passed to xTaskCreateStatic() (in StackType_t words).
 * - thread_prio:         Task priority (UBaseType_t) passed to xTaskCreateStatic().
 */
#define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS                        \
    TaskHandle_t     p_thread_hnd;                                 \
    StaticTask_t     thread_tcb;                                   \
    StackType_t      thread_stack_buf[SERVER_THREAD_STACK_SZ];     \
    TaskFunction_t   p_thread_func;                                \
    uint32_t         thread_stack_size;                            \
    uint32_t         thread_prio;

/*
 ***************************************************************************************************
 * OS PORT FUNCTION DECLARATIONS
 ***************************************************************************************************
 */

#include "fp_csp_binder.h"  /* for fp_cmd_status_t */

/** @brief FreeRTOS pre-request hook — declared here, defined in fp_csp_binder_os.c */
fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout);

#endif /* FP_CSP_BINDER_OS_H */
