find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
qt_standard_project_setup(REQUIRES 6.4)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()
if(POLICY CMP0071)
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 NEW)
endif()

set(QML_IMPORT_PATH "${PROJECT_BINARY_DIR}/src/server;${QML_IMPORT_PATH}" CACHE STRING "For LSP" FORCE)

find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)

add_executable(blur
    main.cpp
)

qt_add_qml_module(blur
    URI Blur
    VERSION "1.0"
    QML_FILES
        Main.qml
    SOURCES
        helper.h
)

target_compile_definitions(blur
    PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    $<$<BOOL:${START_DEMO}>:START_DEMO>
    WLR_USE_UNSTABLE
)

target_link_libraries(blur
    PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    waylibserver
    PkgConfig::PIXMAN
    PkgConfig::WAYLAND
)

if (INSTALL_TINYWL)
    install(TARGETS blur DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
