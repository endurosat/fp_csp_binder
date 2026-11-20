# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- First public release, source-available under the EnduroSat Software License v1.0.
  Earlier versions were released internally only.
- FP request/response handling over CSP, with UDP and RDP server threads and
  synchronous and asynchronous client paths.
- Platform-neutral templates for the OS port (CMSIS-RTOS2, FreeRTOS) and for the
  configuration headers the host project provides.
- Asynchronous pools free a slot in fp's release handler (`fp_dispatch_info_t::p_on_released`),
  which runs after the connection is closed; the README example shows the pattern. Freeing a slot
  in the response handler is no longer correct: the binder can still be using the slot's
  connection parameters at that point.
- Client-path hardening: responses are accepted only from the addressed node,
  and the synchronous wait is bounded by the request timeout.
- Functional tests on Linux: an initialisation test, and adversarial tests over
  the CSP loopback interface that inject spoofed and stray frames. The tests fetch
  `csp-es` and `fp` from their public GitHub repositories at pinned versions.
- Functional test T4: a synchronous request must close its CSP connection exactly once.
- Functional test T5: an asynchronous request's connection is closed exactly once after the
  callback, and only then is `p_active_conn` cleared (reply and timeout cases).
- Functional test T6: a server thread reports to the task monitor after each packet of a
  continuous stream of requests on one connection.
- Asynchronous responses are delivered without the client-session lock: the session tracking
  callback only reads a response and parks it (`p_pending_pkt`), and the new `p_deliver`
  callback dispatches it after the lock is released. Response handlers and release handlers can
  then take application locks and send FP requests. The CSP connection is closed only in the
  session close callback.
- Functional test T7: the response handler and the release handler of an asynchronous request
  run without the client-session lock, and the release handler runs once, after the connection
  is closed (reply and timeout cases).
- Functional test T8: during an asynchronous request, a stray frame from the addressed node with a
  wrong command ID is dropped and freed, the request stays open, and the real reply is delivered
  once.
- The session open callback clears `p_pending_pkt`. A caller that passes its
  `fp_csp_binder_params_t` to fp directly, not through `fp_csp_binder_req_sync` or
  `fp_csp_binder_req_async`, does not have to initialise the field: the session close no longer
  frees whatever it held.
- Functional test T9: a synchronous request whose parameters are set field by field, with
  `p_pending_pkt` left stale, does not have that pointer freed by the session close.
- Functional test T10: a session closed while a response is still parked frees that response once
  and closes its connection once.

### Changed

- **Breaking:** requires fp 1.5.1 or later. fp 1.5.0 added the session `p_deliver` callback and
  the release handler; fp 1.5.1 stops a stray response from counting as delivered. The
  `fp_csp_binder_params_t` struct gains the `p_pending_pkt` field, so code that uses it must be
  rebuilt.

### Fixed

- The server threads now report to the task monitor after each handled packet. Before, a thread
  reported only between connections, so a peer that sent a request in each 50 ms read timeout
  stopped the reports of a thread that worked, and the task monitor of the host project reset the system.
- The synchronous client path no longer closes its CSP connection twice: it now
  clears the connection pointer after closing, so the session close handler skips it.
- README: the asynchronous pool example treated a slot as free as soon as the callback
  cleared `in_use`. A slot reused before the binder had closed its connection lost that
  connection, which then stayed open. The example now frees a slot in fp's release handler,
  which runs after the connection is closed. Applications that copied the old pool pattern
  should do the same.
- An asynchronous request no longer ends on a stray frame from the addressed node whose command
  ID matches no request. The binder used to close the request on such a frame, drop the real
  reply and never call the response handler; with fp 1.5.1 the request stays open and the
  real reply is delivered.
- The request/response trace labelled every transmitted frame as a response, including
  this node's own requests. Each RX and TX line now shows `req`, `resp` or `bcast`, read
  from the frame's FP header.
