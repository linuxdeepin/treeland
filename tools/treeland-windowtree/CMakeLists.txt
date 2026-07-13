# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

option(BUILD_TREELAND_WINDOWTREE "Build the treeland-windowtree Python client" OFF)

if (NOT BUILD_TREELAND_WINDOWTREE)
    return()
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)
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

pybind11_add_module(treeland-windowtree-core MODULE
    src/treeland_windowtree/_core.cpp
    "${windowtree_replica_header}"
)

set_target_properties(treeland-windowtree-core PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/treeland_windowtree"
    OUTPUT_NAME _core
)

target_include_directories(treeland-windowtree-core PRIVATE
    "${windowtree_generated_dir}"
)

target_link_libraries(treeland-windowtree-core PRIVATE
    Python3::Module
    Qt6::Core
    Qt6::RemoteObjects
)

configure_file(
    treeland-windowtree.in
    "${CMAKE_CURRENT_BINARY_DIR}/treeland-windowtree"
    @ONLY
)

install(TARGETS treeland-windowtree-core
    LIBRARY DESTINATION "${Python3_SITELIB}/treeland_windowtree"
    COMPONENT treeland-windowtree
)
install(FILES
    treeland_windowtree/__init__.py
    treeland_windowtree/cli.py
    DESTINATION "${Python3_SITELIB}/treeland_windowtree"
    COMPONENT treeland-windowtree
)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/treeland-windowtree"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT treeland-windowtree
)
