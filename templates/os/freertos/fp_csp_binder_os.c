/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @file fp_csp_binder_os.c  (FreeRTOS port)
 * @brief OS port implementation for fp_csp_binder targeting FreeRTOS.
 *
 * This file provides:
 *   - The UDP and RDP server config instances (csp_server_cfg_t) with FreeRTOS-specific
 *     task handle, static TCB, and stack buffer fields.
 *   - fp_csp_binder_os_start_thread() — creates the server task via xTaskCreateStatic().
 *   - fp_csp_binder_os_pre_request_hook() — placeholder that returns FP_CMD_STATUS_NOERROR.
 *     Replace with a real subsystem-readiness check if your project requires one.
 *
 * Adapt the project-specific parts marked with TODO comments before integrating.
 */

#include <assert.h>
#include "fp_csp_binder_port.h"
#include "fp_csp_binder_cfg.h"

/* TODO: Include the project-specific header that declares runtime_task_handle[] and
 *       the TASK_FP_BINDER_1 index if your project uses a centralised task handle table.
 *       Example: #include "task_handles.h" */

/*
 ***************************************************************************************************
 * SERVER CONFIGURATION INSTANCES
 ***************************************************************************************************
 */

#ifdef ENABLE_UDP_THREAD
/** @brief UDP server configuration — FreeRTOS variant
 *
 *  TODO: If the stack/TCB must reside in a specific memory region (e.g. FRAM), add a
 *        compiler-specific section attribute.  Example for TI CGT:
 *          #pragma DATA_SECTION(udp_server_cfg, ".fram2_extended:bss")
 */
// clang-format off
csp_server_cfg_t udp_server_cfg =
{
    .p_cfg_name        = "UDP/opts:CRC32",
    .backlog_cnt       = CSP_FP_UDP_BACKLOG_CNT,
    .listen_port       = CSP_FP_UDP_SERVER_PORT,
    .opts              = CSP_O_CRC32,
    .csp_params        = { 0 },
    .server_socket     = { 0 },
    .p_thread_name     = "fp-udp-thread",
    .task_mon_id       = TASK_ID_UDP,
    .fp_resp_buf       = { 0 },
    /* FreeRTOS-specific fields */
    .p_thread_func     = &fp_csp_binder_rx_thread_func,
    .p_thread_hnd      = NULL,
    .thread_stack_size = SERVER_THREAD_STACK_SZ,
    .thread_prio       = SERVER_THREAD_PRIO,
};
// clang-format on
#endif /* ENABLE_UDP_THREAD */

#ifdef ENABLE_RDP_THREAD
/** @brief RDP server configuration — FreeRTOS variant */
// clang-format off
csp_server_cfg_t rdp_server_cfg =
{
    .p_cfg_name        = "RDP/opts:CRC32",
    .backlog_cnt       = CSP_FP_RDP_BACKLOG_CNT,
    .listen_port       = CSP_FP_RDP_SERVER_PORT,
    .opts              = CSP_O_RDP | CSP_O_CRC32,
    .csp_params        = { 0 },
    .server_socket     = { 0 },
    .p_thread_name     = "fp-rdp-thread",
    .task_mon_id       = TASK_ID_RDP,
    .fp_resp_buf       = { 0 },
    /* FreeRTOS-specific fields */
    .p_thread_func     = &fp_csp_binder_rx_thread_func,
    .p_thread_hnd      = NULL,
    .thread_stack_size = SERVER_THREAD_STACK_SZ,
    .thread_prio       = SERVER_THREAD_PRIO,
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
    if ((NULL != p_server_cfg->p_thread_hnd) ||
        (NULL == (p_server_cfg->p_thread_hnd = xTaskCreateStatic(
                      p_server_cfg->p_thread_func,
                      p_server_cfg->p_thread_name,
                      p_server_cfg->thread_stack_size,
                      p_server_cfg,
                      p_server_cfg->thread_prio,
                      p_server_cfg->thread_stack_buf,
                      &p_server_cfg->thread_tcb))))
    {
        ES_TRACE_ERROR("could not create thread '%s'", p_server_cfg->p_thread_name);
        return false;
    }

    /* TODO: If your project maintains a centralised task handle table, store the handle here.
     *       Example: runtime_task_handle[TASK_FP_BINDER_1] = p_server_cfg->p_thread_hnd; */

    /* Register task with the task monitoring service */
    TASK_MONITOR_START(p_server_cfg->task_mon_id, p_server_cfg->p_thread_hnd);

    return true;
}

fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout)
{
    /* TODO: If your FreeRTOS project requires a subsystem-readiness check before issuing
     *       FP requests (e.g. waiting for CAN HW or a transport stack to come online),
     *       implement that check here and return FP_CMD_STATUS_TIMEOUT if the system is
     *       not ready within @p timeout milliseconds.
     *
     *       Example using a FreeRTOS event group:
     *         EventBits_t bits = xEventGroupWaitBits(g_subsystem_event_group,
     *                                                FP_SUBSYSTEM_READY_BIT,
     *                                                pdFALSE, pdTRUE,
     *                                                pdMS_TO_TICKS(timeout));
     *         return (bits & FP_SUBSYSTEM_READY_BIT) ? FP_CMD_STATUS_NOERROR
     *                                                : FP_CMD_STATUS_TIMEOUT;
     */
    (void)timeout;
    return FP_CMD_STATUS_NOERROR;
}
