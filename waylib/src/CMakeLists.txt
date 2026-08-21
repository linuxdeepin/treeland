set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(TREELAND_INSTALL_DEV)
    install(FILES
        cmake/WaylibConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Waylib
    )
endif()

add_subdirectory(server)
