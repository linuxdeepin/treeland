# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

option(BUILD_TREELAND_DEBUG "Build the treeland-debug client" ON)

if (NOT BUILD_TREELAND_DEBUG)
    return()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core RemoteObjects Network HttpServer WebSockets)
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

set(treeland_debug_sources
    main.cpp
    debughelpers.cpp
    debugsession.cpp
    "${PROJECT_SOURCE_DIR}/src/modules/resource/treelanddebugsocket.cpp"
    "${windowtree_replica_header}"
)

list(APPEND treeland_debug_sources debugserver.cpp)

qt_add_executable(treeland-debug
    ${treeland_debug_sources}
)

set_target_properties(treeland-debug PROPERTIES
    AUTOMOC ON
)

target_include_directories(treeland-debug PRIVATE
    "${windowtree_generated_dir}"
    "${PROJECT_SOURCE_DIR}/src/modules/resource"
)

target_link_libraries(treeland-debug PRIVATE
    Qt6::Core
    Qt6::RemoteObjects
    Qt6::Network
)

# Qt6::HttpServer and Qt6::WebSockets are required dependencies (see
# find_package above), so the `listen` HTTP/WebSocket server is always built.
target_compile_definitions(treeland-debug PRIVATE TREELAND_DEBUG_LISTEN)
target_link_libraries(treeland-debug PRIVATE
    Qt6::HttpServer
    Qt6::WebSockets
)

install(TARGETS treeland-debug
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT treeland-debug
)
