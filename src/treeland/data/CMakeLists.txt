pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

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

ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/ztreeland-shortcut-manager-v1.xml shortcut-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-socket-manager-v1.xml socket-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-foreign-toplevel-manager-v1.xml foreign-toplevel-manager-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/treeland-personalization-manager-v1.xml personalization-server-protocol)

ws_generate(server wayland-protocols stable/xdg-shell/xdg-shell.xml xdg-shell-protocol)
ws_generate(server wayland-protocols staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml ext-foreign-toplevel-list-server-protocol)

add_library(treeland-data STATIC)

target_sources(treeland-data PRIVATE
    socket-server-protocol.c
    shortcut-server-protocol.c
    personalization-server-protocol.c
    foreign-toplevel-manager-server-protocol.c
    ext-foreign-toplevel-list-server-protocol.c
)

target_link_libraries(treeland-data
    PUBLIC
        PkgConfig::WAYLAND_SERVER
)

target_include_directories(treeland-data PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
