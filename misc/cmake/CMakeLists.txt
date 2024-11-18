include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TreelandConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TreelandConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/treeland
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TreelandConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/treeland
)
