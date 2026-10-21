# fp_csp_binder

[![License](https://img.shields.io/badge/license-EnduroSat%20Software%20License-blue.svg)](LICENSE)

Portable embedded C library that binds the EnduroSat Function Protocol ([FP](https://github.com/endurosat/fp)) to the CubeSat Space Protocol ([csp-es](https://github.com/endurosat/csp-es)), enabling FP request/response handling over CSP transport layers (UDP/RDP).

fp_csp_binder is source-available under the [EnduroSat Software License](LICENSE).

## Architecture

- **Core logic** (`src/`, `inc/`) is OS-agnostic. It binds FP's send and buffer-allocation hooks to CSP,
  runs one CSP server thread per enabled transport (UDP and/or RDP) that dispatches incoming FP requests,
  and provides the client entry points for synchronous, asynchronous and broadcast FP frames.
- **OS port** (`fp_csp_binder_os.h` / `fp_csp_binder_os.c`, provided by the host project) creates the
  server threads and supplies the logging macros. Templates for CMSIS-RTOS2 and FreeRTOS are in
  `templates/os/`.
- **Configuration** (`fp_csp_binder_cfg.h`, provided by the host project) sets ports, thread
  parameters, hook points and assertions. The template is in `templates/cfg/`.

## Requirements

- [fp](https://github.com/endurosat/fp) 1.4.0 or later (CMake target `svc_fp`), with its host
  configuration target `svc_fp_cfg` (see the fp README)
- [csp-es](https://github.com/endurosat/csp-es) 1.4.0 or later (CMake target `csp_es`)
- CMake 3.20 or later (required by csp-es) and a C11 compiler
- FP service bindings generated from Franca IDL (FIDL) by **macchiato**, EnduroSat's source code
  generator, with the `c-gen3` generator, version 1.0 or later. macchiato is being prepared for
  publication at <https://github.com/endurosat/macchiato>; until then, contact
  [EnduroSat](https://www.endurosat.com/contact/) for access.

  ```bash
  java -jar macchiato.jar -g=                                                   # list generators and their versions
  java -jar macchiato.jar --gen=c-gen3 --fdepl=<file>.fdepl --output=<directory> # generate C bindings
  ```

## Integration

### Step 1: Copy the configuration files

The host project provides these files on its include path. Copy them from the templates once and
adapt them; never edit the library sources.

| File | Template | Purpose |
| --- | --- | --- |
| `fp_csp_binder_cfg.h` | `templates/cfg/fp_csp_binder_cfg.h.template` | Ports, thread parameters, task monitor and pre-request hooks, `FP_CFG_CSP_CTX`, assertions |
| `fp_csp_binder_os.h`, `fp_csp_binder_os.c` | `templates/os/cmsis/` or `templates/os/freertos/` | Server thread creation, server configuration instances, logging macros |
| `csp_access.h` | `templates/cfg/csp_access.h.template` | Includes the CSP API (or your project's CSP wrapper) |
| `csp_service_cfg.h` | `templates/cfg/csp_service_cfg.h.template` | `CSP_SERVICE_CFG_BUFFER_SZ`, the FP response buffer size |

```bash
mkdir -p config
cp fp_csp_binder/templates/cfg/fp_csp_binder_cfg.h.template config/fp_csp_binder_cfg.h
cp fp_csp_binder/templates/cfg/csp_access.h.template        config/csp_access.h
cp fp_csp_binder/templates/cfg/csp_service_cfg.h.template   config/csp_service_cfg.h
cp fp_csp_binder/templates/os/cmsis/fp_csp_binder_os.*      config/
# OR for FreeRTOS:
# cp fp_csp_binder/templates/os/freertos/fp_csp_binder_os.* config/
```

fp needs its own configuration (`fp_config.h`, `fp_arch.h`, `fp_config_user.h` and their
implementations), created from the templates in fp's `config/` directory and built as the
`svc_fp_cfg` target. See the fp README.

### Step 2: Add the libraries in CMake

```cmake
# fp configuration: must be defined before fp is added (see the fp README).
# fp builds as a shared library on Linux and Windows, so the configuration must be position-independent.
add_library(svc_fp_cfg STATIC config/fp/fp_config_user.c)  # plus your fp_arch_* implementation
target_include_directories(svc_fp_cfg PUBLIC config/fp external/fp/inc)  # fp headers: svc_fp links svc_fp_cfg, not the reverse
# target_link_libraries(svc_fp_cfg PUBLIC <OS library your fp_arch_* implementation uses>)
set_target_properties(svc_fp_cfg PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(external/csp-es)        # target: csp_es
add_subdirectory(external/fp)            # target: svc_fp
add_subdirectory(external/fp_csp_binder) # targets: fp_csp_binder, fp_csp_binder_deps

# fp_csp_binder configuration headers (Step 1) and OS port
add_library(fp_csp_binder_cfg INTERFACE)
target_include_directories(fp_csp_binder_cfg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/config)
target_link_libraries(fp_csp_binder_deps INTERFACE fp_csp_binder_cfg)
target_sources(fp_csp_binder PRIVATE config/fp_csp_binder_os.c)

target_link_libraries(your_application PRIVATE fp_csp_binder)
```

`fp_csp_binder_deps` links `svc_fp` and `csp_es` by default. Add any further library the
configuration or OS port needs (for example your RTOS or logger target) to it:

```cmake
target_link_libraries(fp_csp_binder_deps INTERFACE my_rtos my_logger)
```

## Configuration

### `fp_csp_binder_cfg.h`

| Macro | Required | Meaning |
| --- | --- | --- |
| `ENABLE_UDP_THREAD`, `ENABLE_RDP_THREAD` | at least one | Start a UDP and/or an RDP server thread |
| `CSP_FP_UDP_SERVER_PORT`, `CSP_FP_UDP_BACKLOG_CNT` | with UDP | CSP port and connection backlog of the UDP server |
| `CSP_FP_RDP_SERVER_PORT`, `CSP_FP_RDP_BACKLOG_CNT` | with RDP | CSP port and connection backlog of the RDP server |
| `FS_MAX_CSP_BUF_SZ_BYTES` | yes | FP response buffer size per server thread (template: `CSP_SERVICE_CFG_BUFFER_SZ`) |
| `SERVER_THREAD_STACK_SZ`, `SERVER_THREAD_PRIO` | yes (read by the OS port) | Stack size and priority of the server threads. CMSIS-RTOS2: bytes and an `osPriority_t`; FreeRTOS: `StackType_t` words and a numeric priority |
| `TASK_ID_UDP`, `TASK_ID_RDP` | yes | Task monitor IDs of the server threads |
| `TASK_MONITOR_INIT(id)`, `TASK_MONITOR_ALIVE_INDICATION(id)`, `TASK_MONITOR_START(id, hnd)` | yes, may be empty | Map to your watchdog or task monitor; `TASK_MONITOR_START` is called by the OS port |
| `FP_CSP_BINDER_PRE_REQUEST_HOOK(timeout)` | no | Called before every synchronous and asynchronous request with a wait limit of 10 000 ms (`timeout`, not the request timeout); return `FP_CMD_STATUS_TIMEOUT` to reject the request. Default: `FP_CMD_STATUS_NOERROR`. The templates map it to `fp_csp_binder_os_pre_request_hook()` |
| `FP_CSP_BINDER_INIT_LOGGER` | no | Expanded once at file scope in each core source file, without a trailing semicolon: leave it empty or make it a complete declaration (for example your logger's per-module registration macro) |
| `FP_CFG_CSP_CTX(p_disp_ctx)` | yes | Attaches the CSP FP system context to a dispatch context. The template default uses `fp_config_user_assign_sys_context()` and `FP_CONFIG_SYS_CTX_TYPE_CSP` from fp's configuration templates |
| `CRIT_ASSERT(cond)`, `BREAK_ASSERT(cond)` | yes, may be empty | Your platform's assertions |

Example of the hook points:

```c
// Task monitoring (optional): map to your watchdog / task monitor service
#define TASK_MONITOR_INIT(task_id)              my_task_mon_init(task_id)
#define TASK_MONITOR_START(task_id, hnd)        my_task_mon_start(task_id, hnd)
#define TASK_MONITOR_ALIVE_INDICATION(task_id)  my_task_mon_alive(task_id)

// Pre-request hook (e.g. wait until the transport is up after reset)
#define FP_CSP_BINDER_PRE_REQUEST_HOOK(timeout) fp_csp_binder_os_pre_request_hook(timeout)
```

### `fp_csp_binder_os.h`

| Macro | Meaning |
| --- | --- |
| `ES_TRACE_ERROR`, `ES_TRACE_WARN`, `ES_TRACE_INFO`, `ES_TRACE_DEBUG` | printf-style logging used by the core. The templates discard the output by default; map them to your logger |
| `FP_CSP_BINDER_OS_SERVER_CFG_FIELDS` | OS-specific thread fields appended to the server configuration structure |

`tests/config/` contains a complete POSIX example of all configuration files: the logging macros are
in `fp_csp_binder_os.h`, the assertions in `fp_csp_binder_cfg.h`.

## Porting to another RTOS

1. Copy `templates/os/cmsis/fp_csp_binder_os.h` and `fp_csp_binder_os.c` as a starting point.
2. Define `FP_CSP_BINDER_OS_SERVER_CFG_FIELDS` with your RTOS thread fields (handle, attributes or TCB, stack).
3. Implement in `fp_csp_binder_os.c`:
   - `bool fp_csp_binder_os_start_thread(csp_server_cfg_t *const p_server_cfg)`: create a thread
     that runs the core's `fp_csp_binder_rx_thread_func(void *p_args)` with `p_server_cfg` as its argument
   - the pre-request hook, if `FP_CSP_BINDER_PRE_REQUEST_HOOK` maps to one
4. Define the server configurations `udp_server_cfg` and/or `rdp_server_cfg` with your RTOS-specific
   thread attributes.

## Initialisation

Initialise CSP (interfaces, routing and the CSP router task) first, then FP and the binder:

```c
#include "fp_csp_binder.h"
#include "fp_config_user.h"
#include "fp_disp_session_mgr.h"
#include "fp_registry.h"

#define APP_MAX_RESP_SZ 256u  /* largest FP response frame (header + payload) a client request may receive */

static fp_dispatch_info_t csp_ref_ctx;  /* used only to reach the CSP system context */

bool app_fp_init(void)
{
    fp_config_user_init();  /* from your fp_config_user.c */

    fp_config_user_assign_sys_context(&csp_ref_ctx, FP_CONFIG_SYS_CTX_TYPE_CSP);
    fp_dispatch_sys_context_t *p_sys_ctx = csp_ref_ctx.p_sys_ctx;

    /* 1. Let FP send and allocate buffers through CSP */
    const fp_csp_binder_sys_cfg_t *p_csp_cfg = fp_csp_binder_get_csp_sys_cfg();
    p_sys_ctx->p_send      = p_csp_cfg->p_send;
    p_sys_ctx->p_alloc_buf = p_csp_cfg->p_buf_alloc;

    /* 2. Session pools; fp_disp_session_mgr_init() clears the session callbacks,
     *    so register the binder's callbacks after it */
    fp_registry_init();
    fp_disp_session_mgr_init(p_sys_ctx);
    for (uint16_t i = 0; i < MAX_CLIENT_SESSIONS_COUNT; i++)
    {
        p_sys_ctx->client_session_pools[i].resp_data_max_size = APP_MAX_RESP_SZ;
    }
    fp_csp_binder_register_callbacks(p_sys_ctx);

    /* 3. Register the generated FIDL interfaces: fp_registry_add_fidl_if(&...) */

    /* 4. Bind the CSP sockets and start the server threads */
    return fp_csp_binder_start();
}
```

Then call `fp_disp_session_mgr_track_sessions(p_sys_ctx)` periodically from a task, every few
milliseconds. It delivers the responses of asynchronous requests to their callbacks, times them out,
and disarms ARM-and-FIRE sessions that were never fired. Without it, asynchronous callbacks never run.

## Building and testing

The functional tests build the library on Linux together with `fp` and `csp-es` and run two tests:

1. **Initialisation test** (`fp_client`): initialises CSP with a ZMQ interface and the binder, and
   starts the binder's server threads.
2. **Adversarial tests** (`fp_adversarial`): a server and a client in one process over the CSP
   loopback interface. Each case injects a stray CSP frame while a request is waiting, and checks
   that the client accepts only the response from the addressed node (spoofing, synchronous and
   asynchronous paths) and that a stray frame does not end the synchronous wait early.

Requirements: CMake 3.20 or later, GCC and G++ (the `fp` build needs a C++ compiler), make, git,
pkg-config, CA certificates for the HTTPS clones, and the libzmq development package. libcjson is
optional (csp-es builds an extra tool with it). On Debian or Ubuntu:

```bash
sudo apt-get install cmake gcc g++ make git pkg-config ca-certificates libzmq3-dev
```

Then run the whole pipeline (clone dependencies, build, test):

```bash
cd tests/scripts
./run_functional_tests.sh
```

The scripts clone `csp-es` and `fp` at pinned release tags; override them with the
`CSP_ES_VERSION` and `FP_VERSION` environment variables. A dependency that is already cloned is
reused, so remove `tests/scripts/deps/` after changing a version. See
[tests/README.md](tests/README.md) for the individual steps.

## Usage Examples

The `fp_csp_binder` module connects the FP (Function Protocol) command layer to the CSP transport. It manages the CSP connection lifecycle so that callers only need to fill in destination and timing parameters, provide the generated request function, and handle the result.

Three entry points are available:

| Function | Direction | When to use |
| --- | --- | --- |
| `fp_csp_binder_req_sync` | client → server | Block until the remote end replies (command/control) |
| `fp_csp_binder_req_async` | client → server | Return immediately; receive the reply in a callback (telemetry polling) |
| `fp_csp_binder_send_bcast` | server → all | Push data to all listeners with no reply expected |

Header: `fp_csp_binder.h`

### Key types

```c
/* Connection parameters — fill this in before every call */
typedef struct {
    uint8_t     prio;          /* CSP message priority (e.g. CSP_PRIO_NORM) */
    uint16_t    dest;          /* destination node address */
    uint8_t     dport;         /* destination node port */
    uint32_t    timeout;       /* milliseconds to wait; overwritten by the binder */
    uint32_t    opts;          /* connection options (e.g. CSP_O_CRC32) */
    csp_conn_t *p_active_conn; /* leave NULL — the binder owns this field */
    void       *p_user_args;   /* optional: pass anything you need in callbacks */
} fp_csp_binder_params_t;

/* Dispatch context — zero-initialise it and pass its address; the binder attaches
 * the CSP system context to it */
fp_dispatch_info_t disp_ctx = { 0 };

/* Return value */
fp_cmd_status_t status;   /* FP_CMD_STATUS_NOERROR on success */
```

For each FIDL method, macchiato generates typed request functions (for example `fp_my_service_set_state_req_sync`). Their request and response parameters are typed pointers, while the binder takes the generic function pointer types `p_client_req_sync_t`, `p_client_req_async_t` and `p_server_bcast_t`, so pass them with a cast as shown in the examples. The binder calls them with the request and response pointers you pass in. Strict tools such as clang's `-Wcast-function-type-strict` report these casts.

### Synchronous request

Use this form when your code must wait for the reply before continuing, such as when sending a control command and checking whether it succeeded.

#### Steps

1. Fill `fp_csp_binder_params_t` with the target node address, port, priority, and options.
2. Zero-initialise the request and response structs; populate request fields.
3. Call `fp_csp_binder_req_sync`, passing `&disp_ctx` as the dispatch context argument, and check the returned status.

#### Example — send a command (no response data needed)

```c
#include "fp_csp_binder.h"
#include "fp_my_service_client.h"   /* generated for your FIDL service */

#define MY_NODE_ADDR   10u
#define MY_DEST_PORT   13u
#define MY_TIMEOUT_MS  2000u

fp_cmd_status_t my_service_send_command(bool enable)
{
    /* Step 1 — connection parameters */
    fp_csp_binder_params_t csp_params = {
        .prio          = CSP_PRIO_NORM,
        .dest          = MY_NODE_ADDR,
        .dport         = MY_DEST_PORT,
        .timeout       = MY_TIMEOUT_MS,   /* binder will overwrite with the call timeout */
        .opts          = CSP_O_CRC32,
        .p_active_conn = NULL,            /* always NULL — do not set this */
        .p_user_args   = NULL,
    };

    /* Step 2 — request / response data */
    fp_dispatch_info_t disp_ctx = { 0 };
    fp_my_service_set_state_req_data_t  req  = {0};
    fp_my_service_set_state_resp_data_t resp = {0};
    req.enable = enable ? MY_SERVICE_STATE_ON : MY_SERVICE_STATE_OFF;

    /* Step 3 — blocking call */
    fp_cmd_status_t status = fp_csp_binder_req_sync(
        FP_FRAME_TYPE_REQ,
        &csp_params,
        (p_client_req_sync_t)&fp_my_service_set_state_req_sync,
        &disp_ctx,
        &req,
        &resp,
        MY_TIMEOUT_MS);

    if (FP_CMD_STATUS_NOERROR != status)
    {
        /* log or handle the error */
        return status;
    }

    /* use resp fields here if needed */
    return FP_CMD_STATUS_NOERROR;
}
```

#### Example — query a value (response data used)

```c
fp_cmd_status_t my_service_read_value(uint32_t *p_out_value)
{
    fp_csp_binder_params_t csp_params = {
        .prio = CSP_PRIO_NORM, .dest = MY_NODE_ADDR, .dport = MY_DEST_PORT,
        .timeout = MY_TIMEOUT_MS, .opts = CSP_O_CRC32,
        .p_active_conn = NULL, .p_user_args = NULL,
    };

    fp_dispatch_info_t disp_ctx = { 0 };
    fp_my_service_get_value_req_data_t  req  = {0};
    fp_my_service_get_value_resp_data_t resp = {0};

    fp_cmd_status_t status = fp_csp_binder_req_sync(
        FP_FRAME_TYPE_REQ,
        &csp_params,
        (p_client_req_sync_t)&fp_my_service_get_value_req_sync,
        &disp_ctx,
        &req,
        &resp,
        MY_TIMEOUT_MS);

    if (FP_CMD_STATUS_NOERROR == status)
    {
        *p_out_value = resp.value;
    }

    return status;
}
```

### Asynchronous request

Use this form when you do not need to block the calling task, for example when periodically polling telemetry from several nodes in a loop. The call returns immediately; the response arrives later in a callback you provide.

Because the call returns before the reply is received, the `fp_csp_binder_params_t` and `fp_dispatch_info_t` **must remain valid until the callback fires**. The usual pattern is to keep these in a small pre-allocated pool and use an `in_use` flag — set before the call, cleared in the callback — to prevent a slot from being reused too early.

#### Async request steps

1. Obtain a free slot from a pool that holds `fp_csp_binder_params_t`, `fp_dispatch_info_t`, and an `in_use` flag.
2. Fill the slot's `csp_params`; set `p_user_args` to point to the `in_use` flag.
3. Call `fp_csp_binder_req_async`, passing `&p_ctx->disp_ctx` as the dispatch context argument, and the response handler callback.
4. In the callback: process the response, then clear the `in_use` flag to release the slot.

#### Pool element definition

```c
typedef struct {
    bool                   in_use;
    fp_csp_binder_params_t csp_params;
    fp_dispatch_info_t     disp_ctx;     /* zeroed by the pool memset below */
    fp_my_service_get_telemetry_req_data_t req_data;  /* request data must also outlive the call */
} my_csp_ctx_t;

#define MY_POOL_SIZE 4u
static my_csp_ctx_t ctx_pool[MY_POOL_SIZE];

static my_csp_ctx_t *get_free_slot(void)
{
    for (uint8_t i = 0; i < MY_POOL_SIZE; i++)
    {
        if (!ctx_pool[i].in_use)
        {
            return &ctx_pool[i];
        }
    }
    return NULL;
}
```

#### Response handler callback

The callback signature must match `fp_generic_response_handler_t`:

```c
void my_service_telemetry_handler(fp_dispatch_info_t            *p_disp_ctx,
                                  const fp_my_service_get_telemetry_resp_data_t *p_resp,
                                  const fp_cmd_status_t          cmd_status)
{
    if (FP_CMD_STATUS_NOERROR == cmd_status && NULL != p_resp)
    {
        /* process p_resp fields here */
        (void)p_resp->temperature;
        (void)p_resp->voltage;
    }

    /* Release the pool slot so it can be reused */
    if (NULL != p_disp_ctx && NULL != p_disp_ctx->p_comm_params)
    {
        fp_csp_binder_params_t *p_csp = p_disp_ctx->p_comm_params;
        if (NULL != p_csp->p_user_args)
        {
            bool *p_in_use = p_csp->p_user_args;
            *p_in_use = false;
        }
    }
}
```

#### Call site

```c
void my_service_poll_telemetry(void)
{
    my_csp_ctx_t *p_ctx = get_free_slot();
    if (NULL == p_ctx)
    {
        /* all slots busy — try again next cycle */
        return;
    }

    /* Step 1 — mark slot busy */
    p_ctx->in_use = true;

    /* Step 2 — connection parameters */
    p_ctx->csp_params.prio          = CSP_PRIO_NORM;
    p_ctx->csp_params.dest          = MY_NODE_ADDR;
    p_ctx->csp_params.dport         = MY_DEST_PORT;
    p_ctx->csp_params.timeout       = MY_TIMEOUT_MS;
    p_ctx->csp_params.opts          = CSP_O_CRC32;
    p_ctx->csp_params.p_active_conn = NULL;             /* always NULL */
    p_ctx->csp_params.p_user_args   = &p_ctx->in_use;  /* callback uses this to free the slot */

    /* Step 3 — non-blocking call */
    fp_cmd_status_t status = fp_csp_binder_req_async(
        FP_FRAME_TYPE_REQ,
        &p_ctx->csp_params,
        (p_client_req_async_t)&fp_my_service_get_telemetry_req_async,
        &p_ctx->disp_ctx,
        &p_ctx->req_data,
        (fp_generic_response_handler_t)&my_service_telemetry_handler,
        MY_TIMEOUT_MS);

    if (FP_CMD_STATUS_NOERROR != status)
    {
        /* send failed — release the slot immediately */
        p_ctx->in_use = false;
    }
}
```

### Broadcast send

Use this form when the node needs to push data to all listeners without expecting a reply, for example publishing telemetry or status updates on a well-known port.

A broadcast is a fire-and-forget operation: there is no response and no timeout parameter. Because the call blocks until the frame has been handed to CSP, a stack-allocated `fp_csp_binder_params_t` and `fp_dispatch_info_t` are fine — the same pattern as a synchronous request.

#### Broadcast steps

1. Fill `fp_csp_binder_params_t` with the destination address (`csp_id_get_max_nodeid()` is the CSP broadcast address of the active CSP version), port, priority, and options. Leave `timeout` at zero or any value — it is not used by the binder for broadcasts.
2. Zero-initialise the broadcast data struct; populate its fields.
3. Call `fp_csp_binder_send_bcast`, passing `&disp_ctx` as the dispatch context argument, and check the returned status.

#### Example — send a broadcast frame

```c
#include "csp/csp_id.h"
#include "fp_csp_binder.h"
#include "fp_my_service_server.h"   /* generated for your FIDL service */

#define MY_BCAST_PORT   20u

fp_cmd_status_t my_service_broadcast_status(uint32_t status_code)
{
    /* Step 1 — connection parameters */
    fp_csp_binder_params_t csp_params = {
        .prio          = CSP_PRIO_NORM,
        .dest          = (uint16_t)csp_id_get_max_nodeid(),  /* CSP broadcast address */
        .dport         = MY_BCAST_PORT,
        .timeout       = 0u,                  /* not used for broadcasts */
        .opts          = CSP_O_CRC32,
        .p_active_conn = NULL,                /* always NULL — do not set this */
        .p_user_args   = NULL,
    };

    /* Step 2 — broadcast data */
    fp_dispatch_info_t disp_ctx = { 0 };
    fp_my_service_status_bcast_data_t bcast = { 0 };
    bcast.status_code = status_code;

    /* Step 3 — send broadcast */
    fp_cmd_status_t status = fp_csp_binder_send_bcast(
        &csp_params,
        (p_server_bcast_t)&fp_my_service_status_send_bcast,
        &disp_ctx,
        &bcast);

    if (FP_CMD_STATUS_NOERROR != status)
    {
        /* log or handle the error */
        return status;
    }

    return FP_CMD_STATUS_NOERROR;
}
```

### Common pitfalls

- **Leave `p_active_conn = NULL`.** The binder owns this field: it resets it on every call and uses it while a request is in progress. Do not write it while an asynchronous request is pending.

- **Check the return value of every call.**
  - `FP_CMD_STATUS_SENDERR`: the request could not be sent (for example no CSP buffer or connection), or a synchronous request got no valid response before the timeout.
  - `FP_CMD_STATUS_TIMEOUT`: the pre-request hook reported that the transport is not ready. An asynchronous request that gets no response returns `FP_CMD_STATUS_NOERROR` from the call; its callback later receives `FP_CMD_STATUS_TIMEOUT` with a NULL response.
  - `FP_CMD_STATUS_NO_MEM`: no free FP client session; raise `MAX_CLIENT_SESSIONS_COUNT` in `fp_config.h` if requests overlap.
  - `FP_CMD_STATUS_INVARGS`: `p_csp_params`, the request function or `p_disp_ctx` is NULL. This is only returned if `BREAK_ASSERT` does not halt.

- **Async: keep the pool slot alive until the callback.** Once `fp_csp_binder_req_async` returns `FP_CMD_STATUS_NOERROR`, do not touch or reuse the slot until the callback clears the `in_use` flag. If the call fails, clear the flag immediately (as shown above). The callback runs only if your application calls `fp_disp_session_mgr_track_sessions()` periodically (see [Initialisation](#initialisation)).

- **Synchronous calls can use the stack.** A local `fp_csp_binder_params_t` and `fp_dispatch_info_t` are fine for synchronous and broadcast calls, because the call returns only when it is done. Asynchronous calls need storage that outlives the call, such as the pool described above.

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for how changes are reviewed
and merged, and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Security

Please report vulnerabilities privately as described in [SECURITY.md](SECURITY.md).

## License

Copyright 2026 ENDUROSAT EAD. fp_csp_binder is source-available under the
[EnduroSat Software License v1.0](LICENSE).

## Support

For questions about EnduroSat products and services, use <https://www.endurosat.com/contact/>.
