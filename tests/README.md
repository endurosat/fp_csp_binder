# FP CSP Binder Functional Tests

This directory contains the functional tests of `fp_csp_binder` on Linux: a POSIX configuration of
the library, the test programs, and the scripts that fetch the dependencies, build and run them.

## Quick Start

### Run All Tests (Recommended)

```bash
cd tests/scripts
./run_functional_tests.sh
```

This script will:
1. Clone the dependencies [csp-es](https://github.com/endurosat/csp-es) and
   [fp](https://github.com/endurosat/fp) at pinned release tags, and check for libzmq and pthread
2. Create the fp configuration files in `config/fp/` from fp's templates
3. Build the library and the test programs
4. Run the tests

### Manual Steps

If you prefer to run steps individually:

```bash
# Step 1: Setup environment (clone dependencies, create the fp configuration)
cd tests/scripts
./setup_environment.sh

# Step 2: Build
./build.sh

# Step 3: Run tests
./run_tests.sh

# Step 4: Cleanup (optional): removes the build directory; --all also removes the dependencies
./cleanup.sh
```

## Directory Structure

```
tests/
├── README.md                          # This file
├── CMakeLists.txt                     # Test build configuration
├── config/
│   ├── fp_csp_binder_cfg.h            # Linux test configuration (also defines the assertions)
│   ├── fp_csp_binder_os.h             # Linux OS port header (defines the logging macros)
│   ├── fp_csp_binder_os.c             # Linux OS port implementation (POSIX threads)
│   ├── csp_access.h                   # Includes the CSP API
│   ├── csp_service_cfg.h              # CSP buffer size for FP responses
│   └── fp/                            # fp configuration, created by setup_environment.sh
├── projects/
│   ├── fp_adversarial/                # Adversarial tests (spoofed and stray frames)
│   ├── fp_client/                     # Initialisation test
│   └── fp_server/                     # FP server example (built, not run by run_tests.sh)
└── scripts/
    ├── run_functional_tests.sh        # Runs setup, build and tests
    ├── setup_environment.sh           # Clones dependencies, creates config/fp/
    ├── create_fp_config_user.sh       # Writes the POSIX fp_config_user.c (called by setup)
    ├── build.sh                       # Configures and builds with CMake
    ├── run_tests.sh                   # Runs the tests
    └── cleanup.sh                     # Removes build output (and dependencies with --all)
```

## Test Configuration

### Linux OS Port (`config/fp_csp_binder_os.*`)

- **POSIX pthreads** for the server threads
- **Logging** to stdout/stderr with `fprintf`
- **No-op pre-request hook**
- **Stack size**: 8 KiB per thread

### Test Configuration (`config/fp_csp_binder_cfg.h`)

- **UDP thread**: enabled on port 12
- **RDP thread**: enabled on port 13
- **Buffer size**: `CSP_SERVICE_CFG_BUFFER_SZ` (1024 bytes) from `config/csp_service_cfg.h`
- **Task monitoring**: disabled (empty macros)

## Tests

### Test 1: Initialisation (`fp_client`)

Initialises CSP with a ZMQ interface, FP and the binder, and starts the binder's server threads.
It passes if every step succeeds. No FP request is sent.

### Test 2: Adversarial (`fp_adversarial`)

Runs an FP server and client in one process over the CSP loopback interface (no ZMQ proxy needed).
Each case injects one stray CSP frame while a request is waiting:

- **T1, synchronous spoofing**: a frame from another node must not be accepted as the response.
- **T2, synchronous stray frame**: a frame with a wrong command ID must not end the wait; the
  request must still complete with `FP_CMD_STATUS_NOERROR`.
- **T3, asynchronous spoofing**: as T1, on the asynchronous path.

## Dependencies

The scripts clone:
- [`csp-es`](https://github.com/endurosat/csp-es): CSP library with ZMQ support
- [`fp`](https://github.com/endurosat/fp): Function Protocol service library

System dependencies (checked, not installed, by the scripts):
- `cmake` 3.20 or later, `make`
- `gcc`, `g++`: C and C++ compilers (the `fp` build requires C++)
- `git`, `ca-certificates`: to clone the dependencies over HTTPS
- `pkg-config`, `libzmq3-dev`: ZMQ library
- `libc6-dev`: POSIX pthread support

## Environment Variables

- `CSP_ES_VERSION`, `FP_VERSION`: release tags to clone (default: `1.4.0`). A dependency that is
  already cloned is reused, so remove the dependencies directory after changing a version.
- `DEPS_DIR`, `BUILD_DIR`: base directories for dependencies and build output (default: the current
  directory). `build.sh`, `run_tests.sh` and `cleanup.sh` use `$DEPS_DIR/deps` and
  `$BUILD_DIR/build`; `setup_environment.sh` takes the base directory as its first argument
  instead. `run_functional_tests.sh` passes `$DEPS_DIR/deps` and `$BUILD_DIR/build` on to them, so
  with it the dependencies end up in `deps/deps/` and the build in `build/build/`.

Example:
```bash
DEPS_DIR=$HOME/fp-deps BUILD_DIR=$HOME/fp-build ./run_functional_tests.sh
```

## Troubleshooting

### Build Fails with "CMake not found" or a CMake version error
Install CMake 3.20 or later: `sudo apt-get install cmake`

### Build Fails with "ZMQ not found"
```bash
sudo apt-get install pkg-config libzmq3-dev
```

### Permission Denied on Scripts
Make scripts executable:
```bash
chmod +x tests/scripts/*.sh
```
