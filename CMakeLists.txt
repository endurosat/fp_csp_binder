# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.15)

project(fp_csp_binder
    VERSION 1.1.0
    LANGUAGES C
    DESCRIPTION "Function Protocol + CubeSat Space Protocol binder library"
)

# Core library (OS-agnostic)
add_library(fp_csp_binder STATIC
    src/fp_csp_binder.c
    src/fp_csp_binder_callbacks.c
)

target_include_directories(fp_csp_binder
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
)

# Create interface target for external dependencies
# Host project can add/override dependencies via:
#   target_link_libraries(fp_csp_binder_deps INTERFACE <additional_libs>)
add_library(fp_csp_binder_deps INTERFACE)

# Default dependencies (can be extended by host project)
target_link_libraries(fp_csp_binder_deps INTERFACE
    svc_fp
    csp_es
)

# Link to main library
target_link_libraries(fp_csp_binder PUBLIC fp_csp_binder_deps)

# C standard
set_target_properties(fp_csp_binder PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

