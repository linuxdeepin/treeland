function(add_pkgconfig_module name library include_dir depends)
    # For cmake
    include(CMakePackageConfigHelpers)
    configure_file(${PROJECT_SOURCE_DIR}/src/cmake/pkgconfig.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/${name}.pc
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${name}.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endfunction()
