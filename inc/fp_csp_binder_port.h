/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_PORT_H
#define FP_CSP_BINDER_PORT_H

/**
 * @file fp_csp_binder_port.h
 * @brief Internal header for fp_csp_binder core — not part of the public API.
 *
 * This header defines the internal `csp_server_cfg_t` structure and the OS abstraction
 * boundary. It must be included by core .c files but NOT by external callers.
 *
 * Each OS port must provide a file named `fp_csp_binder_os.h` on the include path. That
 * file is responsible for:
 *   1. Including the platform headers that provide ES_TRACE_*, BREAK_ASSERT, CRIT_ASSERT.
 *   2. Defining FP_CSP_BINDER_OS_SERVER_CFG_FIELDS — a macro expanding to the OS-specific
 *      struct fields appended at the end of csp_server_cfg_t.
 *
 * FP_CSP_BINDER_PRE_REQUEST_HOOK and FP_CSP_BINDER_INIT_LOGGER are project-level customization
 * points defined in fp_csp_binder_cfg.h, not in the OS port header.
 */

#include "fp_csp_binder.h"
#include "fp_csp_binder_cfg.h"

/* Pull in the OS-specific port header.
 * The consuming project must place this file on the compiler include path. */
#include "fp_csp_binder_os.h"

/*
 ***************************************************************************************************
 * HOOK MACRO DEFAULTS
 * OS ports may override these before this header is processed by defining them in fp_csp_binder_os.h.
 ***************************************************************************************************
 */

/**
 * @brief Pre-request synchronization hook.
 *
 * Called before every fp_csp_binder_req_sync() and fp_csp_binder_req_async() invocation.
 * Its purpose is to block the caller until the underlying communication subsystem (CAN HW,
 * transport stack, etc.) is ready to accept requests. Without this guard there is no
 * buffering — requests issued before the subsystem is online will silently fail.
 *
 * @param timeout  Maximum time in milliseconds to wait for subsystem readiness.
 * @return FP_CMD_STATUS_NOERROR  The subsystem is ready; proceed with the request.
 * @return FP_CMD_STATUS_TIMEOUT  Subsystem did not become ready within the timeout.
 *
 * Default: no-op (returns FP_CMD_STATUS_NOERROR immediately).
 * Override: define this macro in fp_csp_binder_os.h before this file is included.
 */
#ifndef FP_CSP_BINDER_PRE_REQUEST_HOOK
#define FP_CSP_BINDER_PRE_REQUEST_HOOK(timeout)  (FP_CMD_STATUS_NOERROR)
#endif

/**
 * @brief Per-translation-unit logger initializer (if the platform trace system requires it).
 *
 * Default: no-op.
 * Override: define this macro in fp_csp_binder_os.h (e.g. INIT_CMP_LOGGER(true, TRACE_LVL_WARN)).
 */
#ifndef FP_CSP_BINDER_INIT_LOGGER
#define FP_CSP_BINDER_INIT_LOGGER
#endif

/*
 ***************************************************************************************************
 * INTERNAL TYPES
 ***************************************************************************************************
 */

/**
 * @brief CSP server configuration structure.
 *
 * Contains the fields used by the portable core logic, followed by OS-specific fields
 * injected via the FP_CSP_BINDER_OS_SERVER_CFG_FIELDS macro provided by fp_csp_binder_os.h.
 *
 * The OS-specific fields (thread handle, TCB, stack, etc.) are appended at the end so that
 * a pointer to csp_server_cfg_t is also a valid pointer to the common prefix regardless of
 * which OS is in use.
 */
typedef struct
{
    /* --- Common fields (accessed by core logic) --- */
    const char            *p_cfg_name;                                                       /**< user-friendly name of this config (e.g. "UDP+CRC32") */
    fp_csp_binder_params_t csp_params;                                                       /**< CSP-specific communication parameters passed to the FP service */
    csp_socket_t           server_socket;                                                    /**< server socket handle */
    const uint32_t         opts;                                                             /**< CSP listen socket options */
    const uint8_t          listen_port;                                                      /**< CSP listen port */
    const size_t           backlog_cnt;                                                      /**< max number of connection instances to reserve */
    const char            *p_thread_name;                                                    /**< name of the server thread */
    uint16_t               task_mon_id;                                                      /**< task monitor ID for this thread */
    uint8_t                fp_resp_buf[FS_MAX_CSP_BUF_SZ_BYTES] __attribute__((aligned(4))); /**< FP response buffer storage */

    /* --- OS-specific fields (defined by fp_csp_binder_os.h) --- */
    FP_CSP_BINDER_OS_SERVER_CFG_FIELDS
} csp_server_cfg_t;

/*
 ***************************************************************************************************
 * OS PORT INTERFACE
 * Functions and data that must be provided by each OS port (fp_csp_binder_os.c).
 ***************************************************************************************************
 */

/** @brief Generic server thread function — implemented in core, referenced by OS port configs.
 *
 *  Non-static so that OS port files can use its address when initialising server config instances.
 *  @param p_args  Pointer to the owning csp_server_cfg_t instance.
 */
void fp_csp_binder_rx_thread_func(void *p_args);

/** @brief Create and start the OS thread for a server config.
 *
 *  Implemented in the OS port file (fp_csp_binder_os.c).
 *  Called by core after csp_bind() and csp_listen() succeed.
 *
 *  @param[in] p_server_cfg  Pointer to the fully-initialised server config.
 *  @retval true   Thread created and registered successfully.
 *  @retval false  Thread creation failed.
 */
bool fp_csp_binder_os_start_thread(csp_server_cfg_t *const p_server_cfg);

/* Server config instances defined and initialised in the OS port file. */
#ifdef ENABLE_UDP_THREAD
extern csp_server_cfg_t udp_server_cfg;
#endif

#ifdef ENABLE_RDP_THREAD
extern csp_server_cfg_t rdp_server_cfg;
#endif

#endif /* FP_CSP_BINDER_PORT_H */
