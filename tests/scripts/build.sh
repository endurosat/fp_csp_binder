#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

##############################################################################
# build.sh
#
# Builds the fp_csp_binder library and test executables.
#
# Usage: ./build.sh [--build-dir <path>] [--deps-dir <path>]
#
# Environment variables:
#   BUILD_DIR: Build directory (default: ./build)
#   DEPS_DIR: Dependencies directory (default: ./deps)
##############################################################################

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-.}/build"
DEPS_DIR="${DEPS_DIR:-.}"

# If DEPS_DIR doesn't contain csp-es, check if it's in DEPS_DIR/deps
if [ ! -d "$DEPS_DIR/csp-es" ] && [ -d "$DEPS_DIR/deps/csp-es" ]; then
    DEPS_DIR="$DEPS_DIR/deps"
fi

# Convert DEPS_DIR to absolute path
DEPS_DIR="$(cd "$DEPS_DIR" 2>/dev/null && pwd)" || DEPS_DIR="$DEPS_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building FP CSP Binder Tests ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Dependencies directory: $DEPS_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo -e "\n${YELLOW}Configuring CMake...${NC}"
cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR" \
    "$PROJECT_ROOT/tests"

# Build
echo -e "\n${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "\n${GREEN}=== Build complete ===${NC}"
echo "Build artifacts in: $BUILD_DIR"
echo "Next step: Run ./run_tests.sh"

