/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#include <assert.h>
#include <stdbool.h>
#include "fp_csp_binder.h"
#include "fp_csp_binder_port.h"
#include "csp_service_cfg.h"
#include "fp_config_user.h"
#include "fp_disp.h"

FP_CSP_BINDER_INIT_LOGGER

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) ROUTINES DECLARATION
 ***************************************************************************************************
 */

/** @brief Called after a session is opened and fully initialized
 *
 *  @param[in] p_sess Pointer to the session data that was opened
 *
 *  @retval true The session initialization was successful and the session should remain open.
 *  @retval false The session initialization failed and the session should be closed.
 */
static bool fp_csp_binder_session_open_handler(fp_disp_session_base_t *const p_sess);

/** @brief Called during session tracking to check if session should be closed
 *
 *  @param[in] p_sess Pointer to the session data to perform tracking on
 *
 *  @return session user track action
 */
static fp_session_track_action_t fp_csp_binder_session_track_handler(fp_disp_session_base_t *const p_sess);

/** @brief Called before a session is closed to enable users to free resources
 *
 *  @param[in] p_sess Pointer to the session data that is being closed
 */
static void fp_csp_binder_session_close_handler(fp_disp_session_base_t *const p_sess);

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) VARIABLES DEFINITION/DECLARATION
 ***************************************************************************************************
 */

/** @brief Structure containing all CSP binder session lifecycle callbacks */
static const fp_user_session_callbacks_t fp_csp_binder_callbacks = {
    .p_open  = fp_csp_binder_session_open_handler,
    .p_track = fp_csp_binder_session_track_handler,
    .p_close = fp_csp_binder_session_close_handler
};

/*
 ***************************************************************************************************
 * EXTERNAL (NON STATIC) ROUTINES DEFINITION
 ***************************************************************************************************
 */

/** @brief Register the CSP binder session callbacks with the FP service
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 */
void fp_csp_binder_register_callbacks(fp_dispatch_sys_context_t *const p_sys_ctx)
{
    BREAK_ASSERT(NULL != p_sys_ctx);

    fp_disp_session_mgr_register_callbacks(p_sys_ctx, &fp_csp_binder_callbacks);
}

/*
 ***************************************************************************************************
 * INTERNAL (STATIC) ROUTINES DEFINITION
 ***************************************************************************************************
 */

static bool fp_csp_binder_session_open_handler(fp_disp_session_base_t *const p_sess)
{
    BREAK_ASSERT(NULL != p_sess);

    fp_csp_binder_params_t *p_csp_params = (fp_csp_binder_params_t *)p_sess->p_comm_params;

    if (NULL == p_csp_params->p_active_conn)
    {
        ES_TRACE_DEBUG(">>> initiating client connection to node %u:%u", p_csp_params->dest, p_csp_params->dport);
        // no active connection provided to use (most probably this is a brand new client request, hence
        // we have to make a new connection
        p_csp_params->p_active_conn = csp_connect(p_csp_params->prio,
                                                  p_csp_params->dest,
                                                  p_csp_params->dport,
                                                  p_csp_params->timeout,
                                                  p_csp_params->opts);

        if (NULL == p_csp_params->p_active_conn)
        {
            ES_TRACE_ERROR("could not create new connection");
            return false;
        }
    }

    return true;
}

static fp_session_track_action_t fp_csp_binder_session_track_handler(fp_disp_session_base_t *const p_sess)
{
    CRIT_ASSERT(NULL != p_sess);

    // Only handle client sessions of type async
    if (((fp_disp_session_attributes_t *)p_sess->p_session_attr)->sess_type != FP_DISP_SESSION_TYPE_CLIENT)
    {
        return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
    }

    fp_disp_client_session_t *p_client_sess = (fp_disp_client_session_t *)p_sess;

    // Only handle asynchronous client sessions
    if (FP_REQ_TYPE_ASYNC != p_client_sess->req_type)
    {
        return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
    }

    fp_csp_binder_params_t *p_csp_params = (fp_csp_binder_params_t *)p_sess->p_comm_params;

    if (NULL == p_csp_params || NULL == p_csp_params->p_active_conn)
    {
        return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
    }

    // Poll the connection with timeout 0 to see if there is any data pending
    csp_packet_t *p_client_resp_pkt = csp_read(p_csp_params->p_active_conn, 0);

    if (NULL != p_client_resp_pkt)
    {
        ES_TRACE_DEBUG("<A< rcvd %d byte(s) from node addr '%u:%u'", p_client_resp_pkt->length,
                       (unsigned)p_client_resp_pkt->id.src,
                       csp_conn_sport(p_csp_params->p_active_conn));

        // F1: reject packets whose source node doesn't match the configured server
        if ((int)p_csp_params->dest != (int)p_client_resp_pkt->id.src)
        {
            ES_TRACE_WARN("<A< src mismatch: expected=%u got=%u — dropping frame",
                          (unsigned)p_csp_params->dest,
                          (unsigned)p_client_resp_pkt->id.src);
            csp_buffer_free(p_client_resp_pkt);
            return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
        }

        // Create dispatch context for the received packet
        fp_dispatch_info_t disp_ctx = { 0 };
        disp_ctx.data_size     = p_client_resp_pkt->length;
        disp_ctx.p_data        = p_client_resp_pkt->data;
        disp_ctx.p_comm_params = (void *)p_csp_params;

        // try to dispatch the received packet
        if (true == fp_disp_execute(FP_CFG_CSP_CTX((fp_dispatch_info_t *)&disp_ctx)))
        {
            csp_buffer_free(p_client_resp_pkt);

            // Close the connection after receiving a response
            (void)csp_close(p_csp_params->p_active_conn);
            p_csp_params->p_active_conn = NULL;

            // Return true to indicate the session can be closed
            return FP_DISP_TRACK_ACTION_CLOSE_SESSION;
        }
        else
        {
            ES_TRACE_DEBUG("<A< unexpected packet from node addr '%u:%u' - dispatch failed (session still open)",
                            csp_conn_src(p_csp_params->p_active_conn),
                            csp_conn_sport(p_csp_params->p_active_conn));

            // just release the packet buffer but keep the connection open (this is not our expected response)
            // session timeout will continue to be tracked
            csp_buffer_free(p_client_resp_pkt);
        }
    }

    return FP_DISP_TRACK_ACTION_KEEP_SESSION_ACTIVE;
}

static void fp_csp_binder_session_close_handler(fp_disp_session_base_t *const p_sess)
{
    CRIT_ASSERT(NULL != p_sess);

    fp_csp_binder_params_t *p_csp_params = (fp_csp_binder_params_t *)p_sess->p_comm_params;

    // Close any active CSP connection associated with this session
    if (NULL != p_csp_params && NULL != p_csp_params->p_active_conn)
    {
        (void)csp_close(p_csp_params->p_active_conn);
        p_csp_params->p_active_conn = NULL;
    }
}
