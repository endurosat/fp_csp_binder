#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

##############################################################################
# cleanup.sh
#
# Cleans up build artifacts and temporary files.
#
# Usage: ./cleanup.sh [--all]
#
# Options:
#   --all: Also remove dependencies directory
##############################################################################

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-.}/build"
DEPS_DIR="${DEPS_DIR:-.}/deps"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Cleaning up FP CSP Binder Test Environment ===${NC}"

# Remove build directory
if [ -d "$BUILD_DIR" ]; then
    echo "Removing build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓${NC} Build directory removed"
fi

# Remove dependencies if --all flag is set
if [ "$1" = "--all" ]; then
    if [ -d "$DEPS_DIR" ]; then
        echo "Removing dependencies directory: $DEPS_DIR"
        rm -rf "$DEPS_DIR"
        echo -e "${GREEN}✓${NC} Dependencies directory removed"
    fi
    if [ -d "$PROJECT_ROOT/tests/config/fp" ]; then
        echo "Removing generated FP configuration: $PROJECT_ROOT/tests/config/fp"
        rm -rf "$PROJECT_ROOT/tests/config/fp"
        echo -e "${GREEN}✓${NC} FP configuration removed"
    fi
fi

echo -e "\n${GREEN}=== Cleanup complete ===${NC}"

