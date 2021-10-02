set(CMAKE_INCLUDE_CURRENT_DIR ON)

install(FILES
    cmake/WaylibConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Waylib
)

add_subdirectory(server)
