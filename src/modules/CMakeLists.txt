pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)

function(ws_generate_local type input_file output_name)
    find_package(PkgConfig REQUIRED)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    execute_process(COMMAND ${WAYLAND_SCANNER}
        ${type}-header
        ${input_file}
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/${output_name}.h
    )

    execute_process(COMMAND ${WAYLAND_SCANNER}
        public-code
        ${input_file}
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/${output_name}.c
    )
endfunction()

link_libraries(
    PkgConfig::WAYLAND_SERVER
)

add_subdirectory(tools)
add_subdirectory(foreign-toplevel)
add_subdirectory(primary-output)
add_subdirectory(personalization)
add_subdirectory(shortcut)
add_subdirectory(wallpaper-color)
add_subdirectory(window-management)
add_subdirectory(virtual-output)
add_subdirectory(dde-shell)
add_subdirectory(capture)
add_subdirectory(item-selector)
add_subdirectory(ddm)
add_subdirectory(prelaunch-splash)
add_subdirectory(app-id-resolver)
