/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

/*
 * Adversarial test for fp_csp_binder client-path hardening.
 * T1 — F1 (spoofing), sync path   — MUST FAIL before fix
 * T2 — F2 (DoS stray frame), sync — MUST FAIL before fix
 * T3 — F1 (spoofing), async path  — MUST FAIL before fix
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <time.h>

#include "csp/csp.h"
#include "csp/csp_crc32.h"
#include "csp/csp_interface.h"
#include "csp/interfaces/csp_if_lo.h"
#include "fp_csp_binder.h"
#include "fp_csp_binder_cfg.h"
#include "fp_disp.h"
#include "fp_disp_client.h"
#include "fp_disp_session_mgr.h"
#include "fp_registry.h"
#include "fp_proto.h"
#include "fp_config_user.h"

/* -----------------------------------------------------------------------
 * Test constants
 * ----------------------------------------------------------------------- */
/* Node 0 = loopback address (csp_if_lo.addr). All traffic uses the CSP
 * fast-loopback path (csp_io.c:102) so no zmqproxy or external routing is needed. */
#define TEST_SERVER_NODE       (0U)
#define TEST_CLIENT_NODE       (0U)
#define TEST_ATTACKER_NODE     (7U)
#define TEST_SERVER_PORT       (CSP_FP_UDP_SERVER_PORT)
#define SERVER_REPLY_HOLD_MS   (50U)
#define REQUEST_TIMEOUT_MS     (2000U)

#define TEST_IFACE_ID          (0x0042U)
#define TEST_FUNC_ID           (0x00000001UL)

#define KNOWN_PAYLOAD_BYTE     (0xABU)
#define KNOWN_PAYLOAD_SIZE     (4U)

/* Ephemeral sport for the first csp_connect slot (CSP_PORT_MAX_BIND + 1 = 17) */
#define CLIENT_CONN_SPORT      (17U)

/* -----------------------------------------------------------------------
 * T3 async test shared state
 * ----------------------------------------------------------------------- */
typedef struct {
    sem_t             done;
    fp_cmd_status_t   status;
    uint8_t           payload[KNOWN_PAYLOAD_SIZE];
} async_result_t;

static async_result_t *g_async_result = NULL; /* set before each T3 run */

static void async_resp_handler(void                 *p_disp_ctx,
                                uint8_t              *p_untyped_resp,
                                const fp_cmd_status_t cmd_status)
{
    (void)p_disp_ctx;
    if (g_async_result == NULL) { return; }
    g_async_result->status = cmd_status;
    if (p_untyped_resp != NULL)
    {
        /* p_untyped_resp points to the full response buffer (fp_msg_hdr_t + payload).
         * Skip the header to get to the payload bytes. */
        memcpy(g_async_result->payload,
               p_untyped_resp + sizeof(fp_msg_hdr_t),
               KNOWN_PAYLOAD_SIZE);
    }
    sem_post(&g_async_result->done);
}

/* -----------------------------------------------------------------------
 * Shared state
 * ----------------------------------------------------------------------- */
static atomic_uint_least16_t g_client_cmd_id       = ATOMIC_VAR_INIT(UINT16_MAX);
static atomic_int            g_server_handler_fired = ATOMIC_VAR_INIT(0);

/* Shared csp_params so attacker can read p_active_conn → sport after open */
static fp_csp_binder_params_t g_client_csp_params;

/* -----------------------------------------------------------------------
 * FP server dispatch context
 * ----------------------------------------------------------------------- */
static fp_dispatch_info_t g_server_disp_ctx;

/* -----------------------------------------------------------------------
 * FP server request handler — sleeps SERVER_REPLY_HOLD_MS then replies
 * ----------------------------------------------------------------------- */
static void server_handler(fp_dispatch_info_t *p_disp_ctx)
{
    /* Publish cmd_id so attacker can craft a matching spoofed frame (T1).
     * Set g_server_handler_fired AFTER cmd_id so the attacker sees a consistent
     * snapshot: it polls g_server_handler_fired and then reads g_client_cmd_id. */
    const fp_msg_hdr_t *p_req_hdr = (const fp_msg_hdr_t *)p_disp_ctx->p_data;
    atomic_store(&g_client_cmd_id, p_req_hdr->cmd_id);
    atomic_store(&g_server_handler_fired, 1);

    struct timespec hold = { .tv_sec = 0, .tv_nsec = SERVER_REPLY_HOLD_MS * 1000000L };
    nanosleep(&hold, NULL);

    uint8_t reply_buf[sizeof(fp_msg_hdr_t) + KNOWN_PAYLOAD_SIZE];
    fp_msg_hdr_t *p_hdr = (fp_msg_hdr_t *)reply_buf;
    memcpy(p_hdr, p_disp_ctx->p_data, sizeof(fp_msg_hdr_t));
    fp_proto_set_function_type(p_hdr, TEST_FUNC_ID, FP_FRAME_TYPE_RESP);
    p_hdr->status = FP_CMD_STATUS_NOERROR;
    memset(reply_buf + sizeof(fp_msg_hdr_t), KNOWN_PAYLOAD_BYTE, KNOWN_PAYLOAD_SIZE);

    p_disp_ctx->p_data    = reply_buf;
    p_disp_ctx->data_size = (uint32_t)sizeof(reply_buf);

    fp_dispatch_sys_context_t *p_sys_ctx = p_disp_ctx->p_sys_ctx;
    if (p_sys_ctx && p_sys_ctx->p_send)
    {
        p_sys_ctx->p_send(p_disp_ctx);
    }
}

static const fidl_func_t g_test_func_list[] = {
    { .func_id = TEST_FUNC_ID, .p_on_func_cbk = server_handler }
};

static const fidl_if_t g_test_server_fidl = {
    .version     = 1,
    .gen_type    = FIDL_TYPE_SERVER,
    .fidl_if_id  = TEST_IFACE_ID,
    .func_count  = 1,
    .p_func_list = g_test_func_list
};

/* -----------------------------------------------------------------------
 * Client FIDL — needed so fp_disp_execute can route server responses back
 * to the waiting session (via fp_disp_client_handle_resp).
 * In generated code the FIDL tool emits this automatically.
 * ----------------------------------------------------------------------- */
#define TEST_RESP_SIZE  (sizeof(fp_msg_hdr_t) + KNOWN_PAYLOAD_SIZE)

static void client_resp_handler(fp_dispatch_info_t *p_disp_ctx)
{
    fp_disp_client_handle_resp(p_disp_ctx, (uint32_t)TEST_RESP_SIZE);
}

static const fidl_func_t g_test_client_func_list[] = {
    { .func_id = TEST_FUNC_ID, .p_on_func_cbk = client_resp_handler }
};

static const fidl_if_t g_test_client_fidl = {
    .version     = 1,
    .gen_type    = FIDL_TYPE_CLIENT,
    .fidl_if_id  = TEST_IFACE_ID,
    .func_count  = 1,
    .p_func_list = g_test_client_func_list
};

/* -----------------------------------------------------------------------
 * FIDL send-request wrapper matching p_client_req_sync_t:
 *   (disp_ctx, frame_type, p_req, p_resp, timeout)
 * Bridges to fp_disp_client_send_req which needs explicit iface/func IDs.
 * ----------------------------------------------------------------------- */
static fp_cmd_status_t test_send_req(fp_dispatch_info_t *p_disp_ctx,
                                     const fp_frame_type_t frame_type,
                                     void *p_request_data,
                                     void *p_resp,
                                     const uint32_t timeout)
{
    return fp_disp_client_send_req(p_disp_ctx,
                                   TEST_IFACE_ID,
                                   TEST_FUNC_ID,
                                   frame_type,
                                   (uint8_t *)p_request_data,
                                   (uint32_t)sizeof(fp_msg_hdr_t),
                                   (uint8_t *)p_resp,
                                   NULL,
                                   timeout);
}

static fp_cmd_status_t test_send_req_async(fp_dispatch_info_t            *p_disp_ctx,
                                            const fp_frame_type_t          frame_type,
                                            void                          *p_request_data,
                                            fp_generic_response_handler_t  p_resp_handler,
                                            const uint32_t                 timeout)
{
    return fp_disp_client_send_req(p_disp_ctx,
                                   TEST_IFACE_ID,
                                   TEST_FUNC_ID,
                                   frame_type,
                                   (uint8_t *)p_request_data,
                                   (uint32_t)sizeof(fp_msg_hdr_t),
                                   NULL,            /* async: no sync resp buffer */
                                   p_resp_handler,
                                   timeout);
}

/* -----------------------------------------------------------------------
 * Attacker thread: injects an adversarial frame into the CSP routing queue
 * using csp_qfifo_write so we can set id.src to any node address.
 *
 * mode 0 = T1: correct (iface_id, func_id, cmd_id), wrong payload, src=ATTACKER
 * mode 1 = T2: wrong cmd_id (0xDEAD), src=ATTACKER
 * ----------------------------------------------------------------------- */
typedef struct {
    pthread_barrier_t *p_barrier;
    int                mode;
} attacker_args_t;

static void *attacker_thread(void *arg)
{
    attacker_args_t *a = (attacker_args_t *)arg;

    /* Wait until the client fires the request */
    pthread_barrier_wait(a->p_barrier);

    /* Poll until server_handler fires — this guarantees:
     * (a) g_client_cmd_id is published with the real cmd_id, and
     * (b) the CSP connection is open (p_active_conn set before send).
     * Timeout after 1 second to avoid hanging the test. */
    const int POLL_LIMIT = 1000; /* 1000 × 1ms = 1s */
    int polls = 0;
    while (atomic_load(&g_server_handler_fired) == 0 && polls < POLL_LIMIT)
    {
        usleep(1000); /* 1ms */
        polls++;
    }

    /* Read the actual client ephemeral port (idin.dport = sport_outgoing).
     * csp_conn_dport() returns idin.dport which is the port replies must be
     * addressed to — i.e. the client's own outgoing sport. */
    uint8_t client_sport = CLIENT_CONN_SPORT; /* fallback */
    /* C11 ordering: fp_csp_binder_send sets p_active_conn via csp_connect, then
     * server_handler fires and does atomic_store(&g_server_handler_fired, 1).
     * The atomic_load of g_server_handler_fired above establishes a happens-before
     * edge that covers all writes preceding the store, including p_active_conn.
     * No separate lock needed. */
    if (g_client_csp_params.p_active_conn != NULL)
    {
        client_sport = (uint8_t)csp_conn_dport(g_client_csp_params.p_active_conn);
    }

    /* Build FP payload */
    uint8_t adv_buf[sizeof(fp_msg_hdr_t) + KNOWN_PAYLOAD_SIZE];
    memset(adv_buf, 0, sizeof(adv_buf));

    fp_msg_hdr_t *p_hdr = (fp_msg_hdr_t *)adv_buf;
    p_hdr->iface_id = TEST_IFACE_ID;
    fp_proto_set_function_type(p_hdr, TEST_FUNC_ID, FP_FRAME_TYPE_RESP);
    p_hdr->status = FP_CMD_STATUS_NOERROR;

    if (a->mode == 0)
    {
        /* T1: matching cmd_id (read from shared var), wrong payload */
        p_hdr->cmd_id = (uint16_t)atomic_load(&g_client_cmd_id);
        memset(adv_buf + sizeof(fp_msg_hdr_t), 0xFFU, KNOWN_PAYLOAD_SIZE);
    }
    else
    {
        /* T2: mismatched cmd_id — stray frame before legitimate reply */
        p_hdr->cmd_id = 0xDEADU;
        memset(adv_buf + sizeof(fp_msg_hdr_t), 0x00U, KNOWN_PAYLOAD_SIZE);
    }

    /* Allocate a CSP packet and inject it directly into the CSP routing queue.
     * Setting id.src = TEST_ATTACKER_NODE lets us test source-address filtering
     * without needing a separate ZMQ node or proxy process. */
    csp_packet_t *p_pkt = csp_buffer_get(sizeof(adv_buf));
    if (NULL == p_pkt)
    {
        printf("[ATTACKER] csp_buffer_get failed\n");
        return NULL;
    }

    memcpy(p_pkt->data, adv_buf, sizeof(adv_buf));
    p_pkt->length = (uint16_t)sizeof(adv_buf);

    /* CSP header: inject to the client's connection (dport = client's sport).
     * id.src = ATTACKER — this is the node the client should reject (F1 fix). */
    p_pkt->id.pri   = CSP_PRIO_NORM;
    p_pkt->id.src   = TEST_ATTACKER_NODE;
    p_pkt->id.dst   = TEST_CLIENT_NODE;
    p_pkt->id.dport = client_sport;
    p_pkt->id.sport = TEST_SERVER_PORT;
    p_pkt->id.flags = 0;

    /* The client connection requires CRC32 (CSP_O_CRC32 = CSP_SO_CRC32REQ).
     * csp_send() sets the flag + appends the checksum automatically, but we
     * bypass it via csp_qfifo_write, so we must do both steps manually. */
    p_pkt->id.flags |= CSP_FCRC32;
    csp_crc32_append(p_pkt);

    csp_qfifo_write(p_pkt, &csp_if_lo, NULL);

    printf("[ATTACKER] mode=%d frame injected (cmd_id=0x%04X src=%u sport=%u)\n",
           a->mode, p_hdr->cmd_id, TEST_ATTACKER_NODE, (unsigned)client_sport);
    return NULL;
}

/* -----------------------------------------------------------------------
 * Client sync thread
 * ----------------------------------------------------------------------- */
typedef struct {
    pthread_barrier_t *p_barrier;
    fp_cmd_status_t    result;
    uint8_t            resp_buf[sizeof(fp_msg_hdr_t) + KNOWN_PAYLOAD_SIZE];
} client_sync_args_t;

static void *client_sync_thread(void *arg)
{
    client_sync_args_t *c = (client_sync_args_t *)arg;

    fp_dispatch_info_t disp_ctx = { 0 };

    /* Use g_client_csp_params so attacker can read p_active_conn after open */
    g_client_csp_params.prio    = CSP_PRIO_NORM;
    g_client_csp_params.dest    = TEST_SERVER_NODE;
    g_client_csp_params.dport   = TEST_SERVER_PORT;
    g_client_csp_params.timeout = REQUEST_TIMEOUT_MS;
    g_client_csp_params.opts    = CSP_O_CRC32; /* server socket requires CRC32 */
    g_client_csp_params.p_active_conn = NULL;

    fp_config_user_assign_sys_context(&disp_ctx, FP_CONFIG_SYS_CTX_TYPE_CSP);

    uint8_t req_buf[sizeof(fp_msg_hdr_t)] = { 0 };

    /* Signal attacker: connection about to open.
     * The attacker fires during the server's 50 ms hold window. */
    pthread_barrier_wait(c->p_barrier);

    c->result = fp_csp_binder_req_sync(
        FP_FRAME_TYPE_REQ,
        &g_client_csp_params,
        test_send_req,
        &disp_ctx,
        req_buf,
        c->resp_buf,
        REQUEST_TIMEOUT_MS);

    return NULL;
}

/* -----------------------------------------------------------------------
 * Client async thread for T3
 * ----------------------------------------------------------------------- */
typedef struct {
    pthread_barrier_t *p_barrier;
    fp_cmd_status_t    launch_status; /* status returned by fp_csp_binder_req_async itself */
} client_async_args_t;

static void *client_async_thread(void *arg)
{
    client_async_args_t *c = (client_async_args_t *)arg;

    fp_dispatch_info_t disp_ctx = { 0 };

    g_client_csp_params.prio    = CSP_PRIO_NORM;
    g_client_csp_params.dest    = TEST_SERVER_NODE;
    g_client_csp_params.dport   = TEST_SERVER_PORT;
    g_client_csp_params.timeout = REQUEST_TIMEOUT_MS;
    g_client_csp_params.opts    = CSP_O_CRC32;
    g_client_csp_params.p_active_conn = NULL;

    fp_config_user_assign_sys_context(&disp_ctx, FP_CONFIG_SYS_CTX_TYPE_CSP);

    uint8_t req_buf[sizeof(fp_msg_hdr_t)] = { 0 };

    pthread_barrier_wait(c->p_barrier);

    c->launch_status = fp_csp_binder_req_async(
        FP_FRAME_TYPE_REQ,
        &g_client_csp_params,
        test_send_req_async,
        &disp_ctx,
        req_buf,
        async_resp_handler,
        REQUEST_TIMEOUT_MS);

    return NULL;
}

/* -----------------------------------------------------------------------
 * Helper: run one sync test (T1 or T2)
 * Returns 0 on PASS, non-zero on FAIL.
 * ----------------------------------------------------------------------- */
static int run_sync_test(int mode, const char *name)
{
    printf("\n[ADV] === %s ===\n", name);

    atomic_store(&g_client_cmd_id, UINT16_MAX);
    atomic_store(&g_server_handler_fired, 0);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2); /* client + attacker */

    attacker_args_t    att_args = { .p_barrier = &barrier, .mode = mode };
    client_sync_args_t cli_args = {
        .p_barrier = &barrier,
        .result    = FP_CMD_STATUS_SENDERR
    };
    memset(cli_args.resp_buf, 0, sizeof(cli_args.resp_buf));

    pthread_t att_tid, cli_tid;
    pthread_create(&att_tid, NULL, attacker_thread, &att_args);
    pthread_create(&cli_tid, NULL, client_sync_thread, &cli_args);

    pthread_join(cli_tid, NULL);
    pthread_join(att_tid, NULL);
    pthread_barrier_destroy(&barrier);

    int pass = 0;

    if (mode == 0)
    {
        /* T1: must receive NOERROR with KNOWN_PAYLOAD_BYTE in every payload byte */
        if (cli_args.result != FP_CMD_STATUS_NOERROR)
        {
            printf("[ADV] %s FAIL: status=0x%02X (expected NOERROR)\n",
                   name, (unsigned)cli_args.result);
            pass = 1;
        }
        else
        {
            const uint8_t *p_payload = cli_args.resp_buf + sizeof(fp_msg_hdr_t);
            int payload_ok = 1;
            for (int i = 0; i < (int)KNOWN_PAYLOAD_SIZE; i++)
            {
                if (p_payload[i] != KNOWN_PAYLOAD_BYTE)
                {
                    payload_ok = 0;
                    break;
                }
            }
            if (!payload_ok)
            {
                printf("[ADV] %s FAIL: received spoofed/wrong payload\n", name);
                pass = 1;
            }
            else
            {
                printf("[ADV] %s PASS\n", name);
                pass = 0;
            }
        }
    }
    else
    {
        /* T2: must receive NOERROR (not SENDERR/TIMEOUT) after stray frame */
        if (cli_args.result != FP_CMD_STATUS_NOERROR)
        {
            printf("[ADV] %s FAIL: status=0x%02X (expected NOERROR)\n",
                   name, (unsigned)cli_args.result);
            pass = 1;
        }
        else
        {
            printf("[ADV] %s PASS\n", name);
            pass = 0;
        }
    }

    return pass;
}

/* -----------------------------------------------------------------------
 * Helper: run T3 (async spoof / F1 on async path)
 * Returns 0 on PASS, non-zero on FAIL.
 * ----------------------------------------------------------------------- */
static int run_async_test(const char *name)
{
    printf("\n[ADV] === %s ===\n", name);

    atomic_store(&g_client_cmd_id, UINT16_MAX);
    atomic_store(&g_server_handler_fired, 0);

    async_result_t result;
    memset(&result, 0, sizeof(result));
    sem_init(&result.done, 0, 0);
    g_async_result = &result;

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2); /* client + attacker */

    attacker_args_t     att_args = { .p_barrier = &barrier, .mode = 0 /* spoof */ };
    client_async_args_t cli_args = { .p_barrier = &barrier, .launch_status = FP_CMD_STATUS_SENDERR };

    pthread_t att_tid, cli_tid;
    pthread_create(&att_tid, NULL, attacker_thread,     &att_args);
    pthread_create(&cli_tid, NULL, client_async_thread, &cli_args);

    pthread_join(cli_tid, NULL);
    pthread_join(att_tid, NULL);
    pthread_barrier_destroy(&barrier);

    /* Wait for the async response handler (or timeout) */
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec  += (REQUEST_TIMEOUT_MS + 500) / 1000;
    deadline.tv_nsec += (long)(((REQUEST_TIMEOUT_MS + 500) % 1000) * 1000000L);
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }

    int sem_ret = sem_timedwait(&result.done, &deadline);
    sem_destroy(&result.done);
    g_async_result = NULL;

    int pass = 0;

    if (cli_args.launch_status != FP_CMD_STATUS_NOERROR)
    {
        printf("[ADV] %s FAIL: fp_csp_binder_req_async launch status=0x%02X\n",
               name, (unsigned)cli_args.launch_status);
        pass = 1;
    }
    else if (sem_ret != 0)
    {
        printf("[ADV] %s FAIL: response handler never called (timeout)\n", name);
        pass = 1;
    }
    else if (result.status != FP_CMD_STATUS_NOERROR)
    {
        printf("[ADV] %s FAIL: handler status=0x%02X (expected NOERROR)\n",
               name, (unsigned)result.status);
        pass = 1;
    }
    else
    {
        int payload_ok = 1;
        for (int i = 0; i < (int)KNOWN_PAYLOAD_SIZE; i++)
        {
            if (result.payload[i] != KNOWN_PAYLOAD_BYTE)
            {
                payload_ok = 0;
                break;
            }
        }
        if (!payload_ok)
        {
            printf("[ADV] %s FAIL: received spoofed/wrong payload\n", name);
            pass = 1;
        }
        else
        {
            printf("[ADV] %s PASS\n", name);
            pass = 0;
        }
    }

    return pass;
}

/* -----------------------------------------------------------------------
 * CSP router thread: dequeues packets from csp_qfifo and delivers them.
 * Must run for any in-process CSP traffic to work (loopback included).
 * ----------------------------------------------------------------------- */
static void *task_router(void *param)
{
    (void)param;
    while (1) { csp_route_work(); }
    return NULL;
}

static void router_start(void)
{
    pthread_t handle;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&handle, &attr, task_router, NULL);
    pthread_attr_destroy(&attr);
}

/* -----------------------------------------------------------------------
 * Session tracker thread: periodically calls fp_disp_session_mgr_track_sessions
 * so that async client sessions are polled for responses via csp_read(..., 0).
 * Without this thread, fp_csp_binder_session_track_handler is never invoked
 * and async response callbacks never fire.
 * ----------------------------------------------------------------------- */
static void *task_session_tracker(void *param)
{
    fp_dispatch_sys_context_t *p_sys_ctx = (fp_dispatch_sys_context_t *)param;
    struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = 5 * 1000000L }; /* 5 ms */
    while (1)
    {
        fp_disp_session_mgr_track_sessions(p_sys_ctx);
        nanosleep(&sleep_ts, NULL);
    }
    return NULL;
}

static void session_tracker_start(fp_dispatch_sys_context_t *p_sys_ctx)
{
    pthread_t handle;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&handle, &attr, task_session_tracker, p_sys_ctx);
    pthread_attr_destroy(&attr);
}

/* -----------------------------------------------------------------------
 * System init: loopback-only CSP stack — no ZMQ or proxy needed.
 * csp_if_lo (addr=0) is registered by csp_init(); all traffic to dst=0
 * takes the fast loopback path in csp_io.c (bypasses routing table and
 * split-horizon check).
 * ----------------------------------------------------------------------- */
static int init_system(void)
{
    csp_init();
    router_start();

    fp_config_user_init();

    fp_config_user_assign_sys_context(&g_server_disp_ctx, FP_CONFIG_SYS_CTX_TYPE_CSP);
    const fp_csp_binder_sys_cfg_t *p_csp_cfg = fp_csp_binder_get_csp_sys_cfg();
    g_server_disp_ctx.p_sys_ctx->p_send      = p_csp_cfg->p_send;
    g_server_disp_ctx.p_sys_ctx->p_alloc_buf = p_csp_cfg->p_buf_alloc;

    fp_registry_init();
    fp_disp_session_mgr_init(g_server_disp_ctx.p_sys_ctx);

    /* resp_data_max_size is set once here (not by fp_disp_session_mgr_init) */
    for (uint16_t i = 0; i < MAX_CLIENT_SESSIONS_COUNT; i++)
    {
        g_server_disp_ctx.p_sys_ctx->client_session_pools[i].resp_data_max_size =
            (uint32_t)(sizeof(fp_msg_hdr_t) + KNOWN_PAYLOAD_SIZE);
    }

    fp_csp_binder_register_callbacks(g_server_disp_ctx.p_sys_ctx);

    if (!fp_registry_add_fidl_if(&g_test_server_fidl))
    {
        fprintf(stderr, "[ADV] FIDL server registration failed\n");
        return -1;
    }

    if (!fp_registry_add_fidl_if(&g_test_client_fidl))
    {
        fprintf(stderr, "[ADV] FIDL client registration failed\n");
        return -1;
    }

    if (!fp_csp_binder_start())
    {
        fprintf(stderr, "[ADV] fp_csp_binder_start failed\n");
        return -1;
    }

    /* Start the async session tracker — polls csp_read(..., 0) for open async
     * client sessions and invokes fp_csp_binder_session_track_handler. */
    session_tracker_start(g_server_disp_ctx.p_sys_ctx);

    usleep(100000); /* let server threads spin up */
    printf("[ADV] System initialized (server port %u)\n", TEST_SERVER_PORT);
    return 0;
}

int main(void)
{
    printf("[ADV] Adversarial test starting...\n");

    if (init_system() != 0)
    {
        fprintf(stderr, "[ADV] System init FAILED\n");
        return EXIT_FAILURE;
    }

    int failures = 0;
    failures += run_sync_test(0, "T1 (sync spoof / F1)");
    failures += run_sync_test(1, "T2 (sync DoS / F2)");
    failures += run_async_test("T3 (async spoof / F1 async path)");

    if (failures > 0)
    {
        fprintf(stderr, "[ADV] %d test(s) FAILED\n", failures);
        return EXIT_FAILURE;
    }

    printf("[ADV] All tests PASSED\n");
    return EXIT_SUCCESS;
}
