pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)

function(ws_generate_local type input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    execute_process(COMMAND ${WAYLAND_SCANNER}
        ${type}-header
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.h
    )

    execute_process(COMMAND ${WAYLAND_SCANNER}
        public-code
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.c
    )
endfunction()

function(ws_generate type protocols input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

    if(NOT EXISTS input_file)
        set(input_file ${WAYLAND_PROTOCOLS}/${input_file})
    endif()

    execute_process(COMMAND ${WAYLAND_SCANNER}
        ${type}-header
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.h
    )

    execute_process(COMMAND ${WAYLAND_SCANNER}
        public-code
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.c
    )
endfunction()

ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-shortcut-manager-v1.xml shortcut-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-foreign-toplevel-manager-v1.xml foreign-toplevel-manager-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-personalization-manager-v1.xml personalization-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-output-management.xml output-management-protocol)

ws_generate(server wayland-protocols stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)
ws_generate(server wayland-protocols staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml ext-foreign-toplevel-list-server-protocol)
ws_generate(server wayland-protocols staging/ext-session-lock/ext-session-lock-v1.xml ext-session-lock-server-protocol)

add_library(treeland-data STATIC)

target_sources(treeland-data PRIVATE
    shortcut-server-protocol.c
    personalization-server-protocol.c
    foreign-toplevel-manager-server-protocol.c
    ext-foreign-toplevel-list-server-protocol.c
    output-management-protocol.c
    ext-session-lock-server-protocol.c
)

target_link_libraries(treeland-data
    PUBLIC
        PkgConfig::WAYLAND_SERVER
)

target_include_directories(treeland-data PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
