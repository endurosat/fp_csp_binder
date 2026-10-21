# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.15)

project(fp_client
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "FP Client for functional testing"
)

# Create the FP client executable
add_executable(fp_client
    src/main.c
)

# Link against fp_csp_binder and its dependencies
target_link_libraries(fp_client PRIVATE
    fp_csp_binder
    pthread
)

# Set C standard
set_target_properties(fp_client PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Install the executable
install(TARGETS fp_client
    RUNTIME DESTINATION bin
)

