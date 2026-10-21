#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

##############################################################################
# run_functional_tests.sh
#
# Main test runner that orchestrates the entire test pipeline:
# 1. Setup environment (clone dependencies)
# 2. Build all components
# 3. Run functional tests
#
# Usage: ./run_functional_tests.sh [--deps-dir <path>] [--build-dir <path>]
#
# Environment variables:
#   DEPS_DIR: Dependencies directory (default: ./deps)
#   BUILD_DIR: Build directory (default: ./build)
##############################################################################

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="${DEPS_DIR:-.}/deps"
BUILD_DIR="${BUILD_DIR:-.}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   FP CSP Binder Functional Test Suite                      ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

echo "Dependencies directory: $DEPS_DIR"
echo "Build directory: $BUILD_DIR"

##############################################################################
# Step 1: Setup Environment
##############################################################################

echo -e "\n${BLUE}Step 1: Setting up environment...${NC}"
if bash "$SCRIPT_DIR/setup_environment.sh" "$DEPS_DIR"; then
    echo -e "${GREEN}✓ Environment setup complete${NC}"
else
    echo -e "${RED}✗ Environment setup failed${NC}"
    exit 1
fi

##############################################################################
# Step 2: Build
##############################################################################

echo -e "\n${BLUE}Step 2: Building components...${NC}"
if BUILD_DIR="$BUILD_DIR" DEPS_DIR="$DEPS_DIR" bash "$SCRIPT_DIR/build.sh"; then
    echo -e "${GREEN}✓ Build complete${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

##############################################################################
# Step 3: Run Tests
##############################################################################

echo -e "\n${BLUE}Step 3: Running functional tests...${NC}"
if BUILD_DIR="$BUILD_DIR" bash "$SCRIPT_DIR/run_tests.sh"; then
    echo -e "${GREEN}✓ Tests passed${NC}"
else
    echo -e "${RED}✗ Tests failed${NC}"
    exit 1
fi

##############################################################################
# Final Summary
##############################################################################

echo -e "\n${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   All tests completed successfully!                        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

exit 0

