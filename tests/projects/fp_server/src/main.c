/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/**
 * @file main.c
 * @brief Minimal FP server for functional testing.
 *
 * This server:
 * - Initializes CSP with ZMQ transport
 * - Initializes FP service with fp_csp_binder
 * - Listens for incoming FP requests
 * - Processes requests and sends responses
 * - Gracefully shuts down on SIGTERM/SIGINT
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

/* CSP and FP headers */
#include "csp/csp.h"
#include "csp/interfaces/csp_if_zmqhub.h"
#include "fp_disp.h"
#include "fp_csp_binder.h"

/* Global flag for graceful shutdown */
static volatile int g_shutdown = 0;

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int sig)
{
    (void)sig;
    g_shutdown = 1;
    printf("[SERVER] Shutdown signal received\n");
}

/**
 * @brief Initialize CSP with ZMQ transport
 */
static int init_csp(void)
{
    printf("[SERVER] Initializing CSP...\n");

    /* Initialize CSP */
    csp_init();  /* Initialize CSP with default settings */

    /* Add ZMQ interface - connect to zmqproxy on localhost */
    csp_iface_t *zmq_iface = NULL;
    if (csp_zmqhub_init(1, "127.0.0.1", 0, &zmq_iface) != CSP_ERR_NONE)
    {
        fprintf(stderr, "[SERVER] ZMQ interface initialization failed\n");
        return -1;
    }

    printf("[SERVER] CSP initialized successfully\n");
    return 0;
}

/**
 * @brief Initialize FP service
 */
static int init_fp_service(void)
{
    printf("[SERVER] Initializing FP service...\n");

    if (!fp_csp_binder_start())
    {
        fprintf(stderr, "[SERVER] FP service initialization failed\n");
        return -1;
    }

    printf("[SERVER] FP service initialized successfully\n");
    return 0;
}

/**
 * @brief Main server loop
 */
static void server_loop(void)
{
    printf("[SERVER] Server loop started, waiting for requests...\n");

    while (!g_shutdown)
    {
        sleep(1);
    }

    printf("[SERVER] Server loop ended\n");
}

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("[SERVER] FP Server starting...\n");

    /* Setup signal handlers */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Initialize CSP */
    if (init_csp() != 0)
    {
        fprintf(stderr, "[SERVER] Failed to initialize CSP\n");
        return EXIT_FAILURE;
    }

    /* Initialize FP service */
    if (init_fp_service() != 0)
    {
        fprintf(stderr, "[SERVER] Failed to initialize FP service\n");
        return EXIT_FAILURE;
    }

    /* Run server loop */
    server_loop();

    printf("[SERVER] FP Server shutting down...\n");
    return EXIT_SUCCESS;
}

