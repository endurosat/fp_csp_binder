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
- Client-path hardening: responses are accepted only from the addressed node,
  and the synchronous wait is bounded by the request timeout.
- Functional tests on Linux: an initialisation test, and adversarial tests over
  the CSP loopback interface that inject spoofed and stray frames. The tests fetch
  `csp-es` and `fp` from their public GitHub repositories at pinned versions.
- Functional test T4: a synchronous request must close its CSP connection exactly once.
- Functional test T5: an asynchronous request's connection is closed exactly once after the
  callback, and only then is `p_active_conn` cleared (reply and timeout cases).

### Fixed

- The synchronous client path no longer closes its CSP connection twice: it now
  clears the connection pointer after closing, so the session close handler skips it.
- README: the asynchronous pool example treated a slot as free as soon as the callback
  cleared `in_use`. A slot reused before the binder had closed its connection lost that
  connection, which then stayed open. A slot is now free only when `in_use` is false and
  `p_active_conn` is `NULL`. Applications that copied the old pool pattern should add the
  same check.
- The request/response trace labelled every transmitted frame as a response, including
  this node's own requests. Each RX and TX line now shows `req`, `resp` or `bcast`, read
  from the frame's FP header.
