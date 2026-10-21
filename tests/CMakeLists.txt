# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.15)

project(fp_csp_binder_tests
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "Functional tests for fp_csp_binder"
)

# Find or add dependencies
# Look for dependencies in CMAKE_PREFIX_PATH first, then in standard locations

# Determine dependency paths
set(CSP_ES_PATH "${CMAKE_PREFIX_PATH}/csp-es")
set(FP_PATH "${CMAKE_PREFIX_PATH}/fp")

# If CMAKE_PREFIX_PATH doesn't work, try relative paths from tests directory
if(NOT EXISTS "${CSP_ES_PATH}/CMakeLists.txt")
    # Try relative to tests directory
    set(CSP_ES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/deps/csp-es")
    if(NOT EXISTS "${CSP_ES_PATH}/CMakeLists.txt")
        # Try relative to project root
        set(CSP_ES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/csp-es")
    endif()
endif()

if(NOT EXISTS "${FP_PATH}/CMakeLists.txt")
    # Try relative to tests directory
    set(FP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/deps/fp")
    if(NOT EXISTS "${FP_PATH}/CMakeLists.txt")
        # Try relative to project root
        set(FP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/fp")
    endif()
endif()

# Normalize paths
get_filename_component(CSP_ES_PATH "${CSP_ES_PATH}" ABSOLUTE)
get_filename_component(FP_PATH "${FP_PATH}" ABSOLUTE)

# Add csp-es
if(EXISTS "${CSP_ES_PATH}/CMakeLists.txt")
    message(STATUS "Adding csp-es from: ${CSP_ES_PATH}")
    set(CSP_BUILD_SAMPLES OFF CACHE BOOL "Build CSP samples" FORCE)
    add_subdirectory("${CSP_ES_PATH}" "${CMAKE_BINARY_DIR}/csp_es")
else()
    message(FATAL_ERROR "csp-es not found at ${CSP_ES_PATH}")
endif()

# Create FP configuration target before adding fp subdirectory
# This is a static library that includes the architecture-specific implementations
add_library(svc_fp_cfg STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config/fp/fp_config_user.c
)
target_include_directories(svc_fp_cfg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config/fp
    ${FP_PATH}/inc
)
# Compile with -fPIC since this will be linked into a shared library
set_target_properties(svc_fp_cfg PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Link pthread for the POSIX implementation
target_link_libraries(svc_fp_cfg PUBLIC pthread)

# Add fp
if(EXISTS "${FP_PATH}/CMakeLists.txt")
    message(STATUS "Adding fp from: ${FP_PATH}")
    add_subdirectory("${FP_PATH}" svc_fp)
else()
    message(FATAL_ERROR "fp not found at ${FP_PATH}")
endif()

# Create config target that provides fp_csp_binder_cfg.h and fp_csp_binder_os.h
add_library(fp_csp_binder_test_cfg INTERFACE)
target_include_directories(fp_csp_binder_test_cfg INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

# Add the main fp_csp_binder library
add_subdirectory(.. fp_csp_binder)

# Link the config to the main library
target_link_libraries(fp_csp_binder PUBLIC fp_csp_binder_test_cfg)

# Link fp_csp_binder to CSP and FP libraries
# This must be done after add_subdirectory so the target exists
target_link_libraries(fp_csp_binder PUBLIC csp_es svc_fp)

# Also add include directories for compilation
target_include_directories(fp_csp_binder PRIVATE
    ${CSP_ES_PATH}/include
    ${FP_PATH}/inc
)

# Add OS port implementation to the main library
target_sources(fp_csp_binder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/config/fp_csp_binder_os.c
)

# Add test subdirectories
add_subdirectory(projects/fp_server)
add_subdirectory(projects/fp_client)
add_subdirectory(projects/fp_adversarial)

