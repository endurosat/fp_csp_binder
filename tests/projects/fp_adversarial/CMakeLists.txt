# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

cmake_minimum_required(VERSION 3.15)

project(fp_adversarial
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "Adversarial test for fp_csp_binder client-path hardening"
)

add_executable(fp_adversarial
    src/main.c
)

target_link_libraries(fp_adversarial PRIVATE
    fp_csp_binder
    csp_es
    pthread
)

set_target_properties(fp_adversarial PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

install(TARGETS fp_adversarial
    RUNTIME DESTINATION bin
)
