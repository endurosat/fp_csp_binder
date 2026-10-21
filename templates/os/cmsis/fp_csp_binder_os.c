/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @file fp_csp_binder_os.c  (CMSIS RTOS2 port)
 * @brief OS port implementation for fp_csp_binder targeting CMSIS RTOS2.
 *
 * This file provides:
 *   - The UDP and RDP server config instances (csp_server_cfg_t) with CMSIS RTOS2
 *     thread attribute initializers.
 *   - fp_csp_binder_os_start_thread() — creates the server thread via osThreadNew().
 *   - fp_csp_binder_os_pre_request_hook() — placeholder that returns FP_CMD_STATUS_NOERROR.
 *     Replace with a real subsystem-readiness check if your project requires one.
 *
 * Adapt the project-specific parts marked with TODO comments before integrating.
 */

#include <assert.h>
#include "fp_csp_binder_port.h"
#include "fp_csp_binder_cfg.h"

/* TODO: Include the project-specific task monitor header that provides
 *       TASK_MONITOR_START, TASK_MONITOR_INIT, TASK_MONITOR_ALIVE_INDICATION.
 *       These are already mapped to the real functions via fp_csp_binder_cfg.h. */

/*
 ***************************************************************************************************
 * SERVER CONFIGURATION INSTANCES
 ***************************************************************************************************
 */

#ifdef ENABLE_UDP_THREAD
/** @brief UDP server configuration — CMSIS RTOS2 variant */
// clang-format off
csp_server_cfg_t udp_server_cfg =
{
    .p_cfg_name   = "UDP/opts:CRC32",
    .backlog_cnt  = CSP_FP_UDP_BACKLOG_CNT,
    .listen_port  = CSP_FP_UDP_SERVER_PORT,
    .opts         = CSP_O_CRC32,
    .csp_params   = { 0 },
    .server_socket = { 0 },
    .p_thread_name = "fp-udp-thread",
    .task_mon_id  = TASK_ID_UDP,
    .fp_resp_buf  = { 0 },
    /* CMSIS-specific fields */
    .p_thread_func = &fp_csp_binder_rx_thread_func,
    .p_thread_hnd  = NULL,
    .thread_tcb    =
    {
        .attr_bits  = osThreadDetached,
        .name       = "fp-udp-thread",
        .priority   = SERVER_THREAD_PRIO,
        .cb_mem     = NULL,
        .cb_size    = 0U,
        .stack_mem  = NULL,
        .stack_size = SERVER_THREAD_STACK_SZ,
        .tz_module  = 0U,
    },
};
// clang-format on
#endif /* ENABLE_UDP_THREAD */

#ifdef ENABLE_RDP_THREAD
/** @brief RDP server configuration — CMSIS RTOS2 variant */
// clang-format off
csp_server_cfg_t rdp_server_cfg =
{
    .p_cfg_name   = "RDP/opts:CRC32",
    .backlog_cnt  = CSP_FP_RDP_BACKLOG_CNT,
    .listen_port  = CSP_FP_RDP_SERVER_PORT,
    .opts         = CSP_O_RDP | CSP_O_CRC32,
    .csp_params   = { 0 },
    .server_socket = { 0 },
    .p_thread_name = "fp-rdp-thread",
    .task_mon_id  = TASK_ID_RDP,
    .fp_resp_buf  = { 0 },
    /* CMSIS-specific fields */
    .p_thread_func = &fp_csp_binder_rx_thread_func,
    .p_thread_hnd  = NULL,
    .thread_tcb    =
    {
        .attr_bits  = osThreadDetached,
        .name       = "fp-rdp-thread",
        .priority   = SERVER_THREAD_PRIO,
        .cb_mem     = NULL,
        .cb_size    = 0U,
        .stack_mem  = NULL,
        .stack_size = SERVER_THREAD_STACK_SZ,
        .tz_module  = 0U,
    },
};
// clang-format on
#endif /* ENABLE_RDP_THREAD */

/*
 ***************************************************************************************************
 * OS PORT FUNCTION IMPLEMENTATIONS
 ***************************************************************************************************
 */

bool fp_csp_binder_os_start_thread(csp_server_cfg_t *const p_server_cfg)
{
    /* Guard: thread must not have been started already */
    BREAK_ASSERT(NULL == p_server_cfg->p_thread_hnd);

    p_server_cfg->p_thread_hnd = osThreadNew(p_server_cfg->p_thread_func,
                                             p_server_cfg,
                                             &p_server_cfg->thread_tcb);

    if (NULL == p_server_cfg->p_thread_hnd)
    {
        ES_TRACE_ERROR("could not create thread '%s'", p_server_cfg->p_thread_name);
        return false;
    }

    /* Register thread with the task monitoring service */
    TASK_MONITOR_START(p_server_cfg->task_mon_id, p_server_cfg->p_thread_hnd);

    return true;
}

fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout)
{
    /* TODO: If your project requires a subsystem-readiness check before issuing FP requests
     *       (e.g. waiting for CAN HW or a transport stack to come online), implement it here
     *       and return FP_CMD_STATUS_TIMEOUT if the system is not ready within @p timeout
     *       milliseconds. Without such a wait, requests issued during startup fail, because
     *       fp_csp_binder does not buffer requests.
     *
     *       Example using CMSIS RTOS2 event flags (the wait timeout is in kernel ticks):
     *         uint32_t ticks = (timeout * osKernelGetTickFreq()) / 1000U;
     *         uint32_t flags = osEventFlagsWait(g_subsystem_event_flags, FP_SUBSYSTEM_READY_FLAG,
     *                                           osFlagsWaitAny | osFlagsNoClear, ticks);
     *         return ((flags & osFlagsError) == 0U) ? FP_CMD_STATUS_NOERROR
     *                                               : FP_CMD_STATUS_TIMEOUT;
     */
    (void)timeout;
    return FP_CMD_STATUS_NOERROR;
}
