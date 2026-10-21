/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#include "fp_csp_binder.h"
#include "fp_csp_binder_port.h"
#include "csp_service_cfg.h"
#include "csp/arch/csp_time.h"
#include "fp_config_user.h"
#include "fp_disp.h"

#if !defined(ENABLE_UDP_THREAD) && !defined(ENABLE_RDP_THREAD)
#error You must define at least one of the thread options: ENABLE_UDP_THREAD or ENABLE_RDP_THREAD.
#endif

/* Optional per-translation-unit logger init — provided by fp_csp_binder_os.h or defaults to no-op */
FP_CSP_BINDER_INIT_LOGGER

/*
 ***************************************************************************************************
 * INTERNAL DEFINES
 ***************************************************************************************************
 */

/** @brief Maximum time (ms) passed to the pre-request hook to wait for subsystem readiness.
 *
 *  This is the upper bound for how long fp_csp_binder_req_sync() and fp_csp_binder_req_async()
 *  will block waiting for the underlying communication subsystem to come online before issuing
 *  a request. The hook itself (@ref FP_CSP_BINDER_PRE_REQUEST_HOOK) decides how to use this value.
 */
#define MAX_SENSE_WAIT_TIMEOUT_MS (10000U)

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) ROUTINES DECLARATION
 ***************************************************************************************************
 */

/** @brief Spawn a CSP server task with the specified parameters
 *  @param[in] p_server_cfg configuration for the server
 *
 *  @retval true: the server is up and running
 *  @retval false: the server setup failed
 */
static bool csp_start_fp_server(csp_server_cfg_t *const p_server_cfg);

/** @brief Refer to fp_disp_session_mgr.h::p_user_alloc_buffer_func_t type for details */
static void *fp_csp_binder_buff_alloc(uint32_t buf_size, uint8_t **pp_data);

/** @brief Dispatch an FP-received packet over CSP
 *
 *  @param[in] p_pkt pointer to the CSP packet containing the FP frame
 *  @param[in] p_csp_binder_params CSP connection parameters for the received packet
 *
 *  @retval true  fp_disp_execute matched a FIDL handler (packet was dispatched)
 *  @retval false fp_disp_execute found no matching handler (frame ignored)
 */
static bool csp_fp_dispatch_rcvd_packet(csp_packet_t *p_pkt, fp_csp_binder_params_t *p_csp_binder_params);

/** @brief This function will be called by the FP service whenever there is a need for FP frame transmission */
static bool fp_csp_binder_send(void *p_disp_ctx);

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) VARIABLES DEFINITION/DECLARATION
 ***************************************************************************************************
 */

/** @brief CSP system context functions required by FP generated code */
static const fp_csp_binder_sys_cfg_t csp_sys_cfg = {
    .p_send      = &fp_csp_binder_send,
    .p_buf_alloc = &fp_csp_binder_buff_alloc
};

/*
 ***************************************************************************************************
 * EXTERNAL (NON STATIC) ROUTINES DEFINITION
 ***************************************************************************************************
 */

bool fp_csp_binder_start(void)
{
    bool server_thread_res = false;

    TASK_MONITOR_INIT(TASK_ID_UDP);
    TASK_MONITOR_INIT(TASK_ID_RDP);

#ifdef ENABLE_UDP_THREAD
    server_thread_res = csp_start_fp_server(&udp_server_cfg);
#else
    server_thread_res = true;
#endif

#ifdef ENABLE_RDP_THREAD
    server_thread_res = server_thread_res && csp_start_fp_server(&rdp_server_cfg) && server_thread_res;
#endif

    return server_thread_res;
}

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) ROUTINES DEFINITION
 ***************************************************************************************************
 */

static void *fp_csp_binder_buff_alloc(uint32_t buf_size, uint8_t **pp_data)
{
    csp_packet_t *p_csp_packet = csp_buffer_get((size_t)buf_size);

    CRIT_ASSERT(NULL != pp_data);

    if (NULL == p_csp_packet)
    {
        *pp_data = NULL;        
    }
    else
    {
        *pp_data = p_csp_packet->data;

        CRIT_ASSERT(buf_size <= UINT16_MAX);
        p_csp_packet->length = (uint16_t)buf_size;
    }

    return (void *)p_csp_packet;
}

static bool csp_start_fp_server(csp_server_cfg_t *const p_server_cfg)
{
    BREAK_ASSERT(NULL != p_server_cfg);

    p_server_cfg->server_socket.opts = p_server_cfg->opts;

    int32_t csp_res = csp_bind(&p_server_cfg->server_socket,
                               p_server_cfg->listen_port);

    if (CSP_ERR_NONE != csp_res)
    {
        return false;
    }

    csp_res = csp_listen(&p_server_cfg->server_socket,
                         p_server_cfg->backlog_cnt);

    if (CSP_ERR_NONE != csp_res)
    {
        return false;
    }

    /* Delegate OS-specific thread creation to the port */
    return fp_csp_binder_os_start_thread(p_server_cfg);
}

static bool csp_fp_dispatch_rcvd_packet(csp_packet_t *p_pkt, fp_csp_binder_params_t *p_csp_binder_params)
{
    fp_dispatch_info_t disp_ctx = { 0 };

    disp_ctx.data_size = p_pkt->length;
    disp_ctx.p_data    = p_pkt->data;
    // store the active connection handle so that responses can be sent back directly through the same connection
    disp_ctx.p_comm_params = (void *)p_csp_binder_params;

    bool disp_res = fp_disp_execute(
        FP_CFG_CSP_CTX((fp_dispatch_info_t *)&disp_ctx));

    if (!disp_res)
    {
        ES_TRACE_ERROR("dispatch failed with status 0x%02x", ((fp_msg_hdr_t *)disp_ctx.p_data)->status);
    }

    csp_buffer_free(p_pkt);

    return disp_res;
}

/* === FP request/response cmd_id tracing =====================================================
 * Extract the FP iface/func/cmd_id from a packet payload via the packed fp_msg_hdr_t overlay
 * (cmd_id is the per-transaction tag the dual-bus dedup client stamps and the OBC echoes). Logged
 * on both request-in and response-out paths so a lost transaction can be localised:
 * RX-logged + TX-logged => loss is downstream in CSP/CAN TX (e.g. the bus-off mirror); RX-logged
 * only => no response produced; neither => the request never arrived. */
static void fp_diag_ids(const csp_packet_t *p_pkt, uint16_t *p_iface, uint32_t *p_func, uint16_t *p_cmd)
{
    if ((NULL != p_pkt) && (p_pkt->length >= sizeof(fp_msg_hdr_t)))
    {
        const fp_msg_hdr_t *p_hdr = (const fp_msg_hdr_t *)p_pkt->data;
        *p_iface = p_hdr->iface_id;
        *p_func  = p_hdr->func_id;
        *p_cmd   = p_hdr->cmd_id;
    }
    else
    {
        *p_iface = 0xFFFFU;
        *p_func  = 0xFFFFFFFFU;
        *p_cmd   = 0xFFFFU;
    }
}

void fp_csp_binder_rx_thread_func(void *p_args)
{
    csp_packet_t           *p_rpacket    = NULL;
    csp_server_cfg_t *const p_server_cfg = (csp_server_cfg_t *)p_args;

    CRIT_ASSERT(NULL != p_server_cfg);

    while (true)
    {
        // Report to task monitor that this thread is alive
        TASK_MONITOR_ALIVE_INDICATION(p_server_cfg->task_mon_id);

        p_server_cfg->csp_params.p_active_conn = csp_accept(&p_server_cfg->server_socket,
                                                            30000);

        if (NULL != p_server_cfg->csp_params.p_active_conn)
        {
            while (NULL != (p_rpacket = csp_read(p_server_cfg->csp_params.p_active_conn, 50)))
            {
                /* Log the FP cmd_id so a request can be matched RX->TX across the trace. */
                uint16_t rx_if;
                uint32_t rx_fn;
                uint16_t rx_cmd;
                fp_diag_ids(p_rpacket, &rx_if, &rx_fn, &rx_cmd);
                ES_TRACE_INFO("[%s] RX req len=%u iface=%u func=0x%08lx cmd_id=%u",
                              p_server_cfg->p_cfg_name, p_rpacket->length,
                              (unsigned)rx_if, (unsigned long)rx_fn, (unsigned)rx_cmd);

                csp_fp_dispatch_rcvd_packet(p_rpacket, &p_server_cfg->csp_params);
            }

            csp_close(p_server_cfg->csp_params.p_active_conn);
        }
    }
}

static bool fp_csp_binder_send(void *p_disp_ctx)
{
    fp_dispatch_info_t     *p_ctx             = (fp_dispatch_info_t *)p_disp_ctx;
    fp_csp_binder_params_t *p_csp_params      = (fp_csp_binder_params_t *)p_ctx->p_comm_params;
    csp_packet_t           *p_client_resp_pkt = NULL;

    // try to obtain a reference to the external buffer which was already allocated (if any)
    csp_packet_t *p_packet = (csp_packet_t *)p_ctx->p_ext_buf_ref;

    if (NULL == p_packet)
    {
        // send function called with a buffer onto the stack, hence we need to copy it to a new CSP-allocated buffer
        p_packet = csp_buffer_get((size_t)p_ctx->data_size);

        if (NULL == p_packet)
        {
            ES_TRACE_ERROR("could not allocate packet of size %zu", (size_t)(p_ctx->data_size));
            return false;
        }

        BREAK_ASSERT(p_ctx->data_size <= UINT16_MAX);
        p_packet->length = (uint16_t)p_ctx->data_size;

        (void)memcpy(p_packet->data, p_ctx->p_data, (size_t)p_packet->length);
    }

    // Broadcast path: no connection was established (broadcasts are connectionless)
    if (NULL == p_csp_params->p_active_conn)
    {
        ES_TRACE_DEBUG(">>> bcast sending %d byte(s) to node %u:%u",
                       p_packet->length, p_csp_params->dest, p_csp_params->dport);

        csp_sendto(p_csp_params->prio,
                   p_csp_params->dest,
                   p_csp_params->dport,
                   CSP_ANY,
                   p_csp_params->opts,
                   p_packet);

        return true;
    }

    ES_TRACE_DEBUG(">>> sending %d byte(s) to node addr '%u:%u'", p_packet->length, csp_conn_dst(p_csp_params->p_active_conn), csp_conn_dport(p_csp_params->p_active_conn));

    /* Capture FP ids BEFORE csp_send() consumes/frees p_packet, log AFTER. */
    uint16_t tx_if;
    uint32_t tx_fn;
    uint16_t tx_cmd;
    uint16_t tx_len = p_packet->length;
    fp_diag_ids(p_packet, &tx_if, &tx_fn, &tx_cmd);

    csp_send(p_csp_params->p_active_conn, p_packet);

    ES_TRACE_INFO("TX resp len=%u iface=%u func=0x%08lx cmd_id=%u -> %u:%u",
                  (unsigned)tx_len, (unsigned)tx_if, (unsigned long)tx_fn, (unsigned)tx_cmd,
                  (unsigned)csp_conn_dst(p_csp_params->p_active_conn),
                  (unsigned)csp_conn_dport(p_csp_params->p_active_conn));

    fp_disp_client_session_t const *p_cli_session = (fp_disp_client_session_t *)(((NULL != p_ctx->p_assigned_sess) &&
                                                                                  ((const fp_disp_session_attributes_t *)p_ctx->p_assigned_sess->p_session_attr)->sess_type == FP_DISP_SESSION_TYPE_CLIENT)
                                                                                     ? p_ctx->p_assigned_sess
                                                                                     : NULL);

    bool send_status = true;

    if ((NULL != p_cli_session) && (FP_REQ_TYPE_SYNC == p_cli_session->req_type))
    {
        // F2: deadline-bounded loop so stray/injected frames don't kill the session.
        // F1: drop packets whose CSP source address doesn't match the expected server.
        bool got_response     = false;
        uint32_t deadline_ms  = csp_get_ms() + p_csp_params->timeout;

        while (true)
        {
            uint32_t now_ms       = csp_get_ms();
            /* Wrapping unsigned subtraction: correct even when deadline_ms wrapped past
             * UINT32_MAX. If the difference exceeds the original timeout the deadline has
             * already passed (clock advanced past wrap point), so treat as expired. */
            uint32_t remaining_ms = deadline_ms - now_ms;
            if (remaining_ms == 0U || remaining_ms > p_csp_params->timeout) { break; }

            p_client_resp_pkt = csp_read(p_csp_params->p_active_conn, remaining_ms);

            if (NULL == p_client_resp_pkt)
            {
                // csp_read timed out — no more packets will arrive within deadline
                break;
            }

            ES_TRACE_DEBUG("<S< rcvd %d byte(s) from node addr '%u:%u'",
                           p_client_resp_pkt->length,
                           (unsigned)p_client_resp_pkt->id.src,
                           csp_conn_sport(p_csp_params->p_active_conn));

            // F1: reject packets whose source node doesn't match the configured server
            if ((int)p_csp_params->dest != (int)p_client_resp_pkt->id.src)
            {
                ES_TRACE_WARN("<S< src mismatch: expected=%u got=%u — dropping frame",
                              (unsigned)p_csp_params->dest,
                              (unsigned)p_client_resp_pkt->id.src);
                csp_buffer_free(p_client_resp_pkt);
                continue;
            }

            // F2: dispatch — break on successful FIDL match
            if (csp_fp_dispatch_rcvd_packet(p_client_resp_pkt, p_csp_params))
            {
                p_client_resp_pkt = NULL; /* ownership transferred; prevent any future UAF */
                got_response = true;
                break;
            }
        }

        if (!got_response)
        {
            // The synchronous read loop timed out (or all received frames were rejected).
            // The response event will never be signalled — report a send failure so the
            // caller gets FP_CMD_STATUS_SENDERR immediately instead of waiting again.
            ES_TRACE_WARN("<S< sync FP response timed out after %lu ms (node %u:%u)",
                          (unsigned long)p_csp_params->timeout, p_csp_params->dest, p_csp_params->dport);

            send_status = false;
        }

        (void)csp_close(p_csp_params->p_active_conn);
    }
    else
    {
        // leave the asynchronous session to be tracked in a separate task...
        // the connection will be closed by the session monitoring or when an asynchronous response is received
    }

    return send_status;
}

const fp_csp_binder_sys_cfg_t *fp_csp_binder_get_csp_sys_cfg(void)
{
    return &csp_sys_cfg;
}

fp_cmd_status_t fp_csp_binder_req_sync(const fp_frame_type_t frame_type, fp_csp_binder_params_t *const p_csp_params, p_client_req_sync_t p_req_func, fp_dispatch_info_t *p_disp_ctx, void *p_request_data, void *p_resp, const uint32_t timeout)
{
    BREAK_ASSERT(NULL != p_csp_params);
    BREAK_ASSERT(NULL != p_req_func);
    BREAK_ASSERT(NULL != p_disp_ctx);

    if ((NULL == p_csp_params) || (NULL == p_req_func) || (NULL == p_disp_ctx))
    {
        return FP_CMD_STATUS_INVARGS;
    }

    fp_cmd_status_t hook_res = FP_CSP_BINDER_PRE_REQUEST_HOOK(MAX_SENSE_WAIT_TIMEOUT_MS);

    if (FP_CMD_STATUS_NOERROR != hook_res)
    {
        return hook_res;
    }

    // ensure the connection is always NULL for client requests
    p_csp_params->p_active_conn = NULL;

    p_csp_params->timeout = timeout;

    // store CSP-specific connection information for later use by @ref fp_csp_binder_send
    p_disp_ctx->p_comm_params = (void *)p_csp_params;

    return (*p_req_func)(FP_CFG_CSP_CTX((fp_dispatch_info_t *)p_disp_ctx), frame_type, p_request_data, p_resp, timeout);
}

fp_cmd_status_t fp_csp_binder_req_async(const fp_frame_type_t frame_type, fp_csp_binder_params_t *const p_csp_params, p_client_req_async_t p_req_func, fp_dispatch_info_t *p_disp_ctx, void *p_request_data, fp_generic_response_handler_t p_resp_handler, const uint32_t timeout)
{
    BREAK_ASSERT(NULL != p_csp_params);
    BREAK_ASSERT(NULL != p_req_func);
    BREAK_ASSERT(NULL != p_disp_ctx);

    if ((NULL == p_csp_params) || (NULL == p_req_func) || (NULL == p_disp_ctx))
    {
        return FP_CMD_STATUS_INVARGS;
    }

    fp_cmd_status_t hook_res = FP_CSP_BINDER_PRE_REQUEST_HOOK(MAX_SENSE_WAIT_TIMEOUT_MS);

    if (FP_CMD_STATUS_NOERROR != hook_res)
    {
        return hook_res;
    }

    // ensure the connection is always NULL for client requests
    p_csp_params->p_active_conn = NULL;

    p_csp_params->timeout = timeout;

    // store CSP-specific connection information for later use by @ref fp_csp_binder_send
    p_disp_ctx->p_comm_params = (void *)p_csp_params;

    return (*p_req_func)(FP_CFG_CSP_CTX((fp_dispatch_info_t *)p_disp_ctx), frame_type, p_request_data, p_resp_handler, timeout);
}

fp_cmd_status_t fp_csp_binder_send_bcast(fp_csp_binder_params_t *const p_csp_params, p_server_bcast_t p_bcast_func, fp_dispatch_info_t *p_disp_ctx, void *p_bcast_data)
{
    BREAK_ASSERT(NULL != p_csp_params);
    BREAK_ASSERT(NULL != p_bcast_func);
    BREAK_ASSERT(NULL != p_disp_ctx);

    if ((NULL == p_csp_params) || (NULL == p_bcast_func) || (NULL == p_disp_ctx))
    {
        return FP_CMD_STATUS_INVARGS;
    }

    // ensure the connection is always NULL for broadcasts
    p_csp_params->p_active_conn = NULL;

    // store CSP-specific connection information for later use by @ref fp_csp_binder_send
    p_disp_ctx->p_comm_params = (void *)p_csp_params;

    return (*p_bcast_func)(FP_CFG_CSP_CTX((fp_dispatch_info_t *)p_disp_ctx), p_bcast_data) ? FP_CMD_STATUS_NOERROR : FP_CMD_STATUS_SENDERR;
}

fp_cmd_status_t fp_csp_get_dispatch_comm_params(fp_dispatch_info_t *p_disp_ctx, fp_csp_binder_params_t **pp_csp_params)
{
    BREAK_ASSERT(NULL != p_disp_ctx);
    BREAK_ASSERT(NULL != pp_csp_params);
    if ((NULL == pp_csp_params) || (NULL == p_disp_ctx))
    {
        return FP_CMD_STATUS_INVARGS;
    }

    *pp_csp_params = (fp_csp_binder_params_t *)p_disp_ctx->p_comm_params;

    return FP_CMD_STATUS_NOERROR;
}
