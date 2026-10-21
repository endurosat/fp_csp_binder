/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_CFG_H
#define FP_CSP_BINDER_CFG_H

/* Linux test configuration for fp_csp_binder */
#include "csp_service_cfg.h"

/** @brief Spawn a UDP CSP thread to handle incoming FP requests on port @ref CSP_FP_UDP_SERVER_PORT */
#define ENABLE_UDP_THREAD

/** @brief Spawn an RDP CSP thread to handle incoming FP requests on port @ref CSP_FP_RDP_SERVER_PORT */
#define ENABLE_RDP_THREAD

/** @brief Max number of bytes reserved for CSP FP response datagrams */
#define FS_MAX_CSP_BUF_SZ_BYTES (CSP_SERVICE_CFG_BUFFER_SZ)

#ifdef ENABLE_UDP_THREAD
/** @brief Server port for UDP FP communication */
#define CSP_FP_UDP_SERVER_PORT (12)

/** @brief Max number of UDP connections allocated for FP communication */
#define CSP_FP_UDP_BACKLOG_CNT (5)
#endif

#ifdef ENABLE_RDP_THREAD
/** @brief Server port for RDP FP communication (used for ground station exchanges) */
#define CSP_FP_RDP_SERVER_PORT (13)

/** @brief Max number of RDP connections allocated for FP communication */
#define CSP_FP_RDP_BACKLOG_CNT (2)
#endif

/** @brief Number of stack bytes reserved for each server thread */
#define SERVER_THREAD_STACK_SZ (8192)

/** @brief OS thread priority level used for each server thread.
 *  For Linux, this is not used (POSIX pthreads use different priority model).
 */
#define SERVER_THREAD_PRIO (0)

/** @brief Task monitor IDs for the UDP and RDP server threads. */
#define TASK_ID_UDP (0)
#define TASK_ID_RDP (1)

/** @brief Task monitoring macros — no-op for Linux test environment */
#define TASK_MONITOR_INIT(task_id)
#define TASK_MONITOR_START(task_id, p_thread_hnd)
#define TASK_MONITOR_ALIVE_INDICATION(task_id)

/** @brief Logger initializer — no-op for Linux test environment */
#define FP_CSP_BINDER_INIT_LOGGER

/** @brief Hook called before every FP request.
 *  For Linux test environment, this is a no-op.
 */
#define FP_CSP_BINDER_PRE_REQUEST_HOOK(timeout)  fp_csp_binder_os_pre_request_hook(timeout)

/** @brief Critical assertion macro */
#include <stdio.h>
#include <stdlib.h>
#define CRIT_ASSERT(cond) \
    do { if (!(cond)) { fprintf(stderr, "CRITICAL ASSERT FAILED: %s\n", #cond); exit(1); } } while(0)

/** @brief Breakpoint assertion macro */
#define BREAK_ASSERT(cond) \
    do { if (!(cond)) { fprintf(stderr, "BREAK ASSERT FAILED: %s\n", #cond); } } while(0)

/** @brief Macro to get the FP CSP context from dispatch context.
 *  Assigns the CSP system context (with fp_csp_binder_send as p_send) to the
 *  dispatch context when p_sys_ctx is not already set by the caller.
 */
#define FP_CFG_CSP_CTX(p_disp_ctx) \
    (((p_disp_ctx)->p_sys_ctx == NULL) \
        ? (fp_config_user_assign_sys_context((p_disp_ctx), FP_CONFIG_SYS_CTX_TYPE_CSP), (p_disp_ctx)) \
        : (p_disp_ctx))

#endif /* FP_CSP_BINDER_CFG_H */

