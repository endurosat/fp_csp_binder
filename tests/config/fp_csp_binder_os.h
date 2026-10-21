/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_OS_H
#define FP_CSP_BINDER_OS_H

/**
 * @file fp_csp_binder_os.h  (Linux POSIX port)
 * @brief OS port header for fp_csp_binder targeting Linux with POSIX pthreads.
 *
 * This file provides the Linux-specific implementation of the OS abstraction required by
 * fp_csp_binder_port.h using POSIX pthreads for thread management.
 *
 * Responsibilities:
 *   1. Include platform headers that provide logging and assertions.
 *   2. Define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS with Linux-specific thread fields.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

/* Simple logging macros for Linux test environment */
#define ES_TRACE_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define ES_TRACE_WARN(fmt, ...) \
    fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)
#define ES_TRACE_INFO(fmt, ...) \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define ES_TRACE_DEBUG(fmt, ...) \
    fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

/*
 ***************************************************************************************************
 * OS-SPECIFIC STRUCT FIELDS
 * Injected at the end of csp_server_cfg_t by fp_csp_binder_port.h.
 ***************************************************************************************************
 */

/**
 * @brief Linux POSIX thread fields appended to csp_server_cfg_t.
 *
 * - p_thread_hnd:  pthread_t handle for the server thread.
 * - thread_attr:   pthread_attr_t for thread attributes (stack size, etc.).
 * - p_thread_func: pointer to the server thread entry function.
 */
#define FP_CSP_BINDER_OS_SERVER_CFG_FIELDS \
    pthread_t       p_thread_hnd;           \
    pthread_attr_t  thread_attr;            \
    void           *(*p_thread_func)(void *);

/*
 ***************************************************************************************************
 * OS PORT FUNCTION DECLARATIONS
 ***************************************************************************************************
 */

#include "fp_csp_binder.h"  /* for fp_cmd_status_t */

/** @brief Linux pre-request hook implementation — declared here, defined in fp_csp_binder_os.c */
fp_cmd_status_t fp_csp_binder_os_pre_request_hook(uint32_t timeout);

#endif /* FP_CSP_BINDER_OS_H */

