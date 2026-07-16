# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

option(BUILD_TREELAND_DEBUG "Build the treeland-debug client" ON)

if (NOT BUILD_TREELAND_DEBUG)
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core RemoteObjects)

set(windowtree_rep_file
    "${PROJECT_SOURCE_DIR}/src/modules/resource/treelandwindowtree.rep"
)
set(windowtree_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(windowtree_replica_header
    "${windowtree_generated_dir}/rep_treeland_windowtree_replica.h"
)

add_custom_command(
    OUTPUT "${windowtree_replica_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${windowtree_generated_dir}"
    COMMAND "$<TARGET_FILE:Qt6::repc>"
        -i rep
        -o replica
        "${windowtree_rep_file}"
        "${windowtree_replica_header}"
    DEPENDS "${windowtree_rep_file}"
    VERBATIM
)

qt_add_executable(treeland-debug
    main.cpp
    "${windowtree_replica_header}"
)

set_target_properties(treeland-debug PROPERTIES
    AUTOMOC ON
)

target_include_directories(treeland-debug PRIVATE
    "${windowtree_generated_dir}"
)

target_link_libraries(treeland-debug PRIVATE
    Qt6::Core
    Qt6::RemoteObjects
)

install(TARGETS treeland-debug
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT treeland-debug
)
