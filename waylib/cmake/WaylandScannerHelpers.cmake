function(ws_generate type input_file output_name)
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
