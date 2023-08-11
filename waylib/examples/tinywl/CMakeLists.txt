find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
qt_standard_project_setup(REQUIRES 6.4)

set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR}/src/server/ CACHE STRING "" FORCE)
add_definitions(-DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)

add_executable(tinywl-qtquick
    main.cpp
)

qt_add_qml_module(tinywl-qtquick
    URI Tinywl
    VERSION "1.0"
    SOURCES
        helper.h
    QML_FILES
        Main.qml
        ClientWindow.qml
        XdgStackWorkspace.qml
        XdgSurface.qml
        XdgTiledWorkspace.qml
)

target_link_libraries(tinywl-qtquick
    PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    waylibserver
    PkgConfig::PIXMAN
)

# Don't install, using for debug in local
#install(TARGETS tinywl-qtquick DESTINATION ${CMAKE_INSTALL_BINDIR})
