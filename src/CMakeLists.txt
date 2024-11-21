if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()
if(POLICY CMP0071)
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 NEW)
endif()

find_package(Dtk6 REQUIRED COMPONENTS Core Declarative)
if (NOT DISABLE_DDM)
    find_package(DDM REQUIRED COMPONENTS Common)
endif()
find_package(Qt6 CONFIG REQUIRED ShaderTools Concurrent)
find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(LIBINPUT REQUIRED IMPORTED_TARGET libinput)
# qt_finalize_target will collect all executable's private dependencies that are CMake targets

add_subdirectory(core)
add_subdirectory(interfaces)
add_subdirectory(plugins)
add_subdirectory(modules)
if (NOT DISABLE_DDM)
    add_subdirectory(greeter)
endif()
add_subdirectory(treeland-wallpaper)
add_subdirectory(treeland-shortcut)

set(BIN_NAME treeland)

qt_add_executable(${BIN_NAME}
    main.cpp
)

target_include_directories(${BIN_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interfaces/>
)

target_link_libraries(${BIN_NAME} PRIVATE
    Dtk6::Core
    treeland_core
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

## systemd socket

pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

qt_add_executable(treeland-sd
    systemd-socket.cpp
)

target_link_libraries(treeland-sd
    PRIVATE
        Qt6::DBus
        PkgConfig::Systemd
)

install(TARGETS treeland-sd RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
