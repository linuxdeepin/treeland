function(add_cmake_module name library include_dir)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(${PROJECT_SOURCE_DIR}/src/cmake/CMakeConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${name}
    )
    write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/${name}ConfigVersion.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY AnyNewerVersion
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${name}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${name}
    )
endfunction()

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
