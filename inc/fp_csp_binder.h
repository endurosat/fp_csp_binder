/*
 * SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
 * SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
 */

#ifndef FP_CSP_BINDER_H
#define FP_CSP_BINDER_H

#include "fp_disp.h"
#include "fp_disp_session_mgr.h"
#include "csp_access.h"

/** @brief Structure specifying the necessary parameters for a CSP send operation */
typedef struct
{
    uint8_t     prio;          /**< CSP message priority, see @ref csp_prio_t */
    uint16_t    dest;          /**< destination node address */
    uint8_t     dport;         /**< destination node port */
    uint32_t    timeout;       /**< milliseconds to wait for the response */
    uint32_t    opts;          /**< connection options, see @ref CSP_CONNECTION_OPTIONS defines for possible values */
    csp_conn_t *p_active_conn; /**< this field is not provided by the user but reserved for fp_csp_binder internal use */
    void       *p_user_args;   /**< user-specific data communicated through the comm params structure */
} fp_csp_binder_params_t;

/** @brief Structure specifying CSP-specific functions to configure in regard to the FP service system context */
typedef struct
{
    p_user_send_func_t         p_send;      /**< CSP send function pointer */
    p_user_alloc_buffer_func_t p_buf_alloc; /**< CSP buffer allocation function pointer */
} fp_csp_binder_sys_cfg_t;

/** @brief General FP client synchronous request function pointer type */
typedef fp_cmd_status_t (*p_client_req_sync_t)(fp_dispatch_info_t *p_disp_ctx, const fp_frame_type_t frame_type, void *p_request_data, void *p_resp, const uint32_t timeout);

/** @brief General FP client asynchronous request function pointer type */
typedef fp_cmd_status_t (*p_client_req_async_t)(fp_dispatch_info_t *p_disp_ctx, const fp_frame_type_t frame_type, void *p_request_data, fp_generic_response_handler_t p_resp_handler, const uint32_t timeout);

/** @brief General FP server broadcast send function pointer type */
typedef bool (*p_server_bcast_t)(fp_dispatch_info_t *p_disp_ctx, void *p_bcast_data);

/** @brief Start the FP service
 *
 *  @retval true: binder successfully initialized
 *  @retval false: binder initialization failed
 * */
bool fp_csp_binder_start(void);

/** @brief Register the CSP binder session callbacks with the FP service
 *
 *  @param[in] p_sys_ctx pointer to a valid FP system context
 */
void fp_csp_binder_register_callbacks(fp_dispatch_sys_context_t *const p_sys_ctx);

/** @brief Get a reference to the CSP-specific system configuration
 *
 *  @return pointer to a CSP system configuration context
 */
const fp_csp_binder_sys_cfg_t *fp_csp_binder_get_csp_sys_cfg(void);

/**
 * @brief Call to perform a synchronous FP request over CSP
 *
 * This function wraps FP synchronous requests. Before executing the request it calls the
 * OS-port-defined pre-request hook (@ref FP_CSP_BINDER_PRE_REQUEST_HOOK) which can be used to
 * block until the underlying communication subsystem is ready (e.g. CAN HW, transport stack).
 *
 * @param[in] frame_type FP frame type (typically @ref FP_FRAME_TYPE_REQ)
 * @param[in] p_csp_params Pointer to CSP connection parameters (destination, port, timeout, etc.)
 * @param[in] p_req_func Pointer to the generated FP synchronous request function
 * @param[in] p_disp_ctx Pointer to FP dispatch context
 * @param[in] p_request_data Pointer to request data structure
 * @param[out] p_resp Pointer to response data structure
 * @param[in] timeout Timeout in milliseconds for the request
 *
 * @retval FP_CMD_STATUS_NOERROR Request completed successfully
 * @retval FP_CMD_STATUS_INVARGS Invalid arguments (NULL pointers)
 * @retval FP_CMD_STATUS_TIMEOUT Pre-request hook timeout or request timeout
 */
fp_cmd_status_t fp_csp_binder_req_sync(const fp_frame_type_t frame_type, fp_csp_binder_params_t *const p_csp_params, p_client_req_sync_t p_req_func, fp_dispatch_info_t *p_disp_ctx, void *p_request_data, void *p_resp, const uint32_t timeout);

/**
 * @brief Call to perform an asynchronous FP request over CSP
 *
 * This function wraps FP asynchronous requests. Before executing the request it calls the
 * OS-port-defined pre-request hook (@ref FP_CSP_BINDER_PRE_REQUEST_HOOK) which can be used to
 * block until the underlying communication subsystem is ready (e.g. CAN HW, transport stack).
 *
 * @param[in] frame_type FP frame type (typically @ref FP_FRAME_TYPE_REQ)
 * @param[in] p_csp_params Pointer to CSP connection parameters (destination, port, timeout, etc.)
 * @param[in] p_req_func Pointer to the generated FP asynchronous request function
 * @param[in] p_disp_ctx Pointer to FP dispatch context
 * @param[in] p_request_data Pointer to request data structure
 * @param[in] p_resp_handler Pointer to response handler callback function
 * @param[in] timeout Timeout in milliseconds for the request
 *
 * @retval FP_CMD_STATUS_NOERROR Request initiated successfully
 * @retval FP_CMD_STATUS_INVARGS Invalid arguments (NULL pointers)
 * @retval FP_CMD_STATUS_TIMEOUT Pre-request hook timeout or request timeout
 */
fp_cmd_status_t fp_csp_binder_req_async(const fp_frame_type_t frame_type, fp_csp_binder_params_t *const p_csp_params, p_client_req_async_t p_req_func, fp_dispatch_info_t *p_disp_ctx, void *p_request_data, fp_generic_response_handler_t p_resp_handler, const uint32_t timeout);

/** @brief Call to perform an FP server broadcast over CSP
 *  @param[in] p_csp_params destination end point information and CSP connection settings to be applied for the broadcast
 *  @param[in] p_req_func pointer to the generated FP broadcast send function
 *
 *  @return status of the send operation
 */
fp_cmd_status_t fp_csp_binder_send_bcast(fp_csp_binder_params_t *const p_csp_params, p_server_bcast_t p_bcast_func, fp_dispatch_info_t *p_disp_ctx, void *p_bcast_data);

/** @brief Extract CSP communication parameters from FP dispatch context
 *
 *  This function retrieves the CSP-specific communication parameters that were
 *  stored in the FP dispatch context during request processing. It allows FP
 *  service implementations to access the original CSP connection information
 *  for response handling or connection management.
 *
 *  @param[in] p_disp_ctx Pointer to the FP dispatch context containing communication parameters
 *  @param[out] p_csp_params Pointer to store the extracted CSP communication parameters
 *
 *  @retval FP_CMD_STATUS_NOERROR: Parameters extracted successfully
 *  @retval FP_CMD_STATUS_INVARGS: Invalid arguments (NULL pointers)
 */
fp_cmd_status_t fp_csp_get_dispatch_comm_params(fp_dispatch_info_t *p_disp_ctx, fp_csp_binder_params_t **pp_csp_params);

#endif /* FP_CSP_BINDER_H */
