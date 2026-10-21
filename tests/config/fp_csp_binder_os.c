/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @file fp_csp_binder_os.c  (Linux POSIX port)
 * @brief OS port implementation for fp_csp_binder targeting Linux with POSIX pthreads.
 *
 * This file provides:
 *   - The UDP and RDP server config instances (csp_server_cfg_t) with POSIX pthread
 *     attribute initializers.
 *   - fp_csp_binder_os_start_thread() — creates the server thread via pthread_create().
 *   - fp_csp_binder_os_pre_request_hook() — simple no-op for Linux test environment.
 */

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include "fp_csp_binder_port.h"
#include "fp_csp_binder_cfg.h"

/*
 ***************************************************************************************************
 * POSIX THREAD WRAPPER
 ***************************************************************************************************
 */

/**
 * @brief POSIX-compatible wrapper for fp_csp_binder_rx_thread_func.
 *
 * pthread_create() requires a thread function with signature void *(*)(void *).
 * The generic fp_csp_binder_rx_thread_func returns void, so this wrapper adapts it.
 */
static void *fp_csp_binder_rx_thread_func_posix(void *arg)
{
    fp_csp_binder_rx_thread_func(arg);
    return NULL;
}

/*
 ***************************************************************************************************
 * SERVER CONFIGURATION INSTANCES
 ***************************************************************************************************
 */

#ifdef ENABLE_UDP_THREAD
/** @brief UDP server configuration — Linux POSIX variant */
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
    /* Linux-specific fields */
    .p_thread_func = &fp_csp_binder_rx_thread_func_posix,
    .p_thread_hnd  = 0,
    .thread_attr   = { 0 },
};
#endif /* ENABLE_UDP_THREAD */

#ifdef ENABLE_RDP_THREAD
/** @brief RDP server configuration — Linux POSIX variant */
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
    /* Linux-specific fields */
    .p_thread_func = &fp_csp_binder_rx_thread_func_posix,
    .p_thread_hnd  = 0,
    .thread_attr   = { 0 },
};
#endif /* ENABLE_RDP_THREAD */

/*
 ***************************************************************************************************
 * OS PORT FUNCTION IMPLEMENTATIONS
 ***************************************************************************************************
 */

bool fp_csp_binder_os_start_thread(csp_server_cfg_t *const p_server_cfg)
{
    int ret;

    /* Initialize thread attributes */
    pthread_attr_init(&p_server_cfg->thread_attr);
    pthread_attr_setdetachstate(&p_server_cfg->thread_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&p_server_cfg->thread_attr, SERVER_THREAD_STACK_SZ);

    /* Create the thread */
    ret = pthread_create(&p_server_cfg->p_thread_hnd,
                         &p_server_cfg->thread_attr,
                         p_server_cfg->p_thread_func,
                         p_server_cfg);

    pthread_attr_destroy(&p_server_cfg->thread_attr);

    if (ret != 0)
    {
        ES_TRACE_ERROR("could not create thread '%s': %d", p_server_cfg->p_thread_name, ret);
        return false;
    }

    ES_TRACE_INFO("thread '%s' created successfully", p_server_cfg->p_thread_name);
    return true;
}

fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout)
{
    /* For Linux test environment, no subsystem readiness check needed.
     * In a real embedded system, this would wait for CAN HW, transport stack, etc. */
    (void)timeout;  /* unused */
    return FP_CMD_STATUS_NOERROR;
}

