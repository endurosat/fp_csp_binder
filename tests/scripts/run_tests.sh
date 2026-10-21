#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

##############################################################################
# run_tests.sh
#
# Runs the functional tests for fp_csp_binder.
#
# Usage: ./run_tests.sh [--build-dir <path>]
#
# Environment variables:
#   BUILD_DIR: Build directory (default: ./build)
##############################################################################

set -e
set -o pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-.}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Running FP CSP Binder Functional Tests ===${NC}"
echo "Build directory: $BUILD_DIR"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}"
    echo "Please run ./build.sh first"
    exit 1
fi

# Check if executables exist
if [ ! -f "$BUILD_DIR/projects/fp_server/fp_server" ]; then
    echo -e "${RED}Error: fp_server executable not found${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/projects/fp_client/fp_client" ]; then
    echo -e "${RED}Error: fp_client executable not found${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/projects/fp_adversarial/fp_adversarial" ]; then
    echo -e "${RED}Error: fp_adversarial executable not found${NC}"
    exit 1
fi

##############################################################################
# Test 1: Compilation Test
##############################################################################

echo -e "\n${YELLOW}=== Test 1: Compilation Test ===${NC}"
echo "Verifying that all components compile together..."

# Run the client (which tests initialization)
echo -e "\n${YELLOW}Running FP Client (compilation test)...${NC}"
if "$BUILD_DIR/projects/fp_client/fp_client"; then
    echo -e "${GREEN}✓ Compilation test PASSED${NC}"
    COMPILATION_TEST_PASSED=1
else
    echo -e "${RED}✗ Compilation test FAILED${NC}"
    COMPILATION_TEST_PASSED=0
fi

##############################################################################
# Test 2: Adversarial (client-path hardening)
##############################################################################

echo -e "\n${YELLOW}=== Test 2: Adversarial (T1 sync spoof / F1, T2 sync DoS / F2, T3 async spoof / F1) ===${NC}"
if "$BUILD_DIR/projects/fp_adversarial/fp_adversarial"; then
    echo -e "${GREEN}✓ Adversarial test PASSED${NC}"
    ADVERSARIAL_TEST_PASSED=1
else
    echo -e "${RED}✗ Adversarial test FAILED${NC}"
    ADVERSARIAL_TEST_PASSED=0
fi

##############################################################################
# Summary
##############################################################################

echo -e "\n${GREEN}=== Test Summary ===${NC}"

if [ $COMPILATION_TEST_PASSED -eq 1 ] && [ $ADVERSARIAL_TEST_PASSED -eq 1 ]; then
    echo -e "${GREEN}✓ All tests PASSED${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests FAILED${NC}"
    exit 1
fi

