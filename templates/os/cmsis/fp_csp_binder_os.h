/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_OS_H
#define FP_CSP_BINDER_OS_H

/**
 * @file fp_csp_binder_os.h  (CMSIS RTOS2 port)
 * @brief OS port header for fp_csp_binder targeting CMSIS RTOS2.
 *
 * This file is the CMSIS-specific implementation of the OS abstraction required by
 * fp_csp_binder_port.h. Copy it into your project's include path under the name
 * "fp_csp_binder_os.h" (or adjust the include path so it is found before any other
 * file of the same name).
 *
 * Responsibilities:
 *   1. Provide the ES_TRACE_* logging macros (see PLATFORM LOGGING below).
 *   2. Define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS with the CMSIS-specific thread fields.
 *
 * The assertion macros BREAK_ASSERT and CRIT_ASSERT are defined in fp_csp_binder_cfg.h.
 */

#include <stdbool.h>
#include <stdint.h>
#include "cmsis_os2.h"

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
 * @brief CMSIS RTOS2 thread fields appended to csp_server_cfg_t.
 *
 * - p_thread_hnd:  handle returned by osThreadNew(); used for task monitoring registration.
 * - thread_tcb:    osThreadAttr_t holding stack size, priority, name, and attribute bits.
 *                  The CMSIS kernel uses the attribute to create the thread — no separate stack
 *                  buffer is needed (the kernel allocates it from the heap or a configured pool).
 * - p_thread_func: pointer to the server thread entry function (fp_csp_binder_rx_thread_func).
 */
#define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS \
    osThreadId_t   p_thread_hnd;           \
    osThreadAttr_t thread_tcb;             \
    osThreadFunc_t p_thread_func;

/*
 ***************************************************************************************************
 * OS PORT FUNCTION DECLARATIONS
 ***************************************************************************************************
 */

#include "fp_csp_binder.h"  /* for fp_cmd_status_t */

/** @brief CMSIS pre-request hook implementation — declared here, defined in fp_csp_binder_os.c */
fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout);

#endif /* FP_CSP_BINDER_OS_H */
