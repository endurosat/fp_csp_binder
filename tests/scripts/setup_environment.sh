#!/bin/bash
# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DEPS_DIR="${1:-.}/deps"
CSP_ES_VERSION="${CSP_ES_VERSION:-1.4.0}"
FP_VERSION="${FP_VERSION:-1.4.0}"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'
echo -e "${GREEN}=== FP CSP Binder Test Environment Setup ===${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Dependencies directory: $DEPS_DIR"
echo -e "\n${YELLOW}Checking for required tools...${NC}"
for tool in cmake git gcc make; do
    if command -v $tool &> /dev/null; then
        echo -e "${GREEN}✓${NC} $tool found"
    else
        echo -e "${RED}✗${NC} $tool not found"
        exit 1
    fi
done
echo -e "\n${YELLOW}Setting up dependencies...${NC}"
mkdir -p "$DEPS_DIR"
if [ ! -d "$DEPS_DIR/csp-es" ]; then
    git clone --recursive --branch "$CSP_ES_VERSION" https://github.com/endurosat/csp-es "$DEPS_DIR/csp-es"
    echo -e "${GREEN}✓${NC} csp-es cloned"
else
    echo -e "${GREEN}✓${NC} csp-es already present"
fi
if [ ! -d "$DEPS_DIR/fp" ]; then
    git clone --recursive --branch "$FP_VERSION" https://github.com/endurosat/fp "$DEPS_DIR/fp"
    echo -e "${GREEN}✓${NC} fp cloned"
else
    echo -e "${GREEN}✓${NC} fp already present"
fi
echo -e "\n${YELLOW}Checking for system dependencies...${NC}"
if pkg-config --exists libzmq; then
    echo -e "${GREEN}✓${NC} libzmq found"
else
    echo -e "${YELLOW}⚠${NC} libzmq not found"
fi
if gcc -print-file-name=libpthread.so | grep -q libpthread; then
    echo -e "${GREEN}✓${NC} pthread found"
else
    echo -e "${YELLOW}⚠${NC} pthread not found"
fi
echo -e "\n${YELLOW}Creating FP configuration files...${NC}"
mkdir -p "$PROJECT_ROOT/tests/config/fp"
cp "$DEPS_DIR/fp/config/fp_arch.h.template" "$PROJECT_ROOT/tests/config/fp/fp_arch.h"
echo -e "${GREEN}✓${NC} fp_arch.h created"
cp "$DEPS_DIR/fp/config/fp_config.h.template" "$PROJECT_ROOT/tests/config/fp/fp_config.h"
echo -e "${GREEN}✓${NC} fp_config.h created"
cp "$DEPS_DIR/fp/config/fp_config_user.h.template" "$PROJECT_ROOT/tests/config/fp/fp_config_user.h"
echo -e "${GREEN}✓${NC} fp_config_user.h created"
bash "$SCRIPT_DIR/create_fp_config_user.sh" "$PROJECT_ROOT/tests/config/fp"
echo -e "${GREEN}✓${NC} fp_config_user.c created"
echo -e "\n${GREEN}=== Environment setup complete ===${NC}"
