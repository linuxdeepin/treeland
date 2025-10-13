find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
find_package(Qt6 COMPONENTS QuickPrivate QUIET)
qt_standard_project_setup(REQUIRES 6.8)

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()
if(POLICY CMP0071)
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 NEW)
endif()

set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR}/src/server/ CACHE STRING "" FORCE)
option(START_DEMO "Start demo when boot" ON)

find_package(PkgConfig REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)

set(TARGET tinywl-qtquick)

add_executable(${TARGET}
    main.cpp
)

qt_add_qml_module(${TARGET}
    URI Tinywl
    VERSION "2.0"

    SOURCES helper.h helper.cpp
    SOURCES surfacewrapper.h surfacewrapper.cpp
    SOURCES workspace.h workspace.cpp
    SOURCES output.h output.cpp
    SOURCES qmlengine.h qmlengine.cpp
    SOURCES surfacecontainer.h surfacecontainer.cpp
    SOURCES layersurfacecontainer.h layersurfacecontainer.cpp
    SOURCES rootsurfacecontainer.h rootsurfacecontainer.cpp
    SOURCES surfaceproxy.h surfaceproxy.cpp
    SOURCES wallpaperprovider.h wallpaperprovider.cpp
    SOURCES wallpaperimage.h wallpaperimage.cpp
    SOURCES workspacemodel.h workspacemodel.cpp

    QML_FILES PrimaryOutput.qml
    QML_FILES CopyOutput.qml
    QML_FILES TitleBar.qml
    QML_FILES Decoration.qml
    QML_FILES TaskBar.qml
    QML_FILES RoundedClipEffect.qml
    QML_FILES SurfaceContent.qml
    QML_FILES Shadow.qml
    QML_FILES Border.qml
    QML_FILES GeometryAnimation.qml
    QML_FILES OutputMenuBar.qml
    QML_FILES WorkspaceSwitcher.qml
    QML_FILES WorkspaceProxy.qml
    QML_FILES WindowMenu.qml

    RESOURCES
    "res/xx.jpg"
)

target_compile_definitions(${TARGET}
    PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    PROJECT_BINARY_DIR="${PROJECT_BINARY_DIR}"
    $<$<BOOL:${START_DEMO}>:START_DEMO>
)

target_link_libraries(${TARGET}
    PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickPrivate
    Waylib::WaylibServer
    PkgConfig::PIXMAN
    PkgConfig::WAYLAND
)

if (INSTALL_TINYWL)
    install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
