if(NOT DEFINED WAYLAND_PROTOCOLS_OUTPUTDIR)
    set(WAYLAND_PROTOCOLS_OUTPUTDIR ${CMAKE_BINARY_DIR}/protocols)
    file(MAKE_DIRECTORY ${WAYLAND_PROTOCOLS_OUTPUTDIR})
    message(STATUS "Create wayland protocols outputdir: ${WAYLAND_PROTOCOLS_OUTPUTDIR}")
    include_directories(${WAYLAND_PROTOCOLS_OUTPUTDIR})
else()
    message(STATUS "Setting wayland protocols outputdir: ${WAYLAND_PROTOCOLS_OUTPUTDIR}")
endif()

function(ws_generate type protocols input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_PROTOCOLS ${protocols} pkgdatadir)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

    if(NOT EXISTS ${input_file})
        set(input_file ${WAYLAND_PROTOCOLS}/${input_file})
    endif()

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
