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
