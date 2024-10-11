if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()
if(POLICY CMP0071)
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 NEW)
endif()

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
find_package(Dtk6 REQUIRED COMPONENTS Core Declarative)
find_package(DDM REQUIRED COMPONENTS Common)
find_package(Qt6 CONFIG REQUIRED ShaderTools)
find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
# qt_finalize_target will collect all executable's private dependencies that are CMake targets

add_subdirectory(modules)
add_subdirectory(greeter)
# add_subdirectory(treeland-wallpaper)
# add_subdirectory(treeland-shortcut)

qt_add_dbus_adaptor(DBUS_ADAPTOR "${CMAKE_SOURCE_DIR}/misc/dbus/org.deepin.compositor1.xml" "treeland.h" TreeLand::TreeLand)

set(BIN_NAME treeland)

qt_add_executable(${BIN_NAME}
    main.cpp
    # ${DBUS_ADAPTOR}
)

# qt_add_resources(${BIN_NAME} "treeland_assets"
#     PREFIX "/dsg"
#     BASE ${PROJECT_RESOURCES_DIR}
#     FILES
#         ${PROJECT_RESOURCES_DIR}/icons/close.dci
#         ${PROJECT_RESOURCES_DIR}/icons/multitaskview_close.dci
# )

target_include_directories(${BIN_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/treeland
)

qt_add_qml_module(${BIN_NAME}
    URI Treeland
    VERSION "2.0"

    SOURCES
        treeland/cmdline.h
        treeland/cmdline.cpp
        treeland/helper.h
        treeland/helper.cpp
        treeland/surfacewrapper.h
        treeland/surfacewrapper.cpp
        treeland/workspace.h
        treeland/workspace.cpp
        treeland/output.h
        treeland/output.cpp
        treeland/qmlengine.h
        treeland/qmlengine.cpp
        treeland/surfacecontainer.h
        treeland/surfacecontainer.cpp
        treeland/layersurfacecontainer.h
        treeland/layersurfacecontainer.cpp
        treeland/lockscreen.h
        treeland/lockscreen.cpp
        treeland/rootsurfacecontainer.h
        treeland/rootsurfacecontainer.cpp
        treeland/surfaceproxy.h
        treeland/surfaceproxy.cpp
        treeland/treeland.h
        treeland/treeland.cpp
        treeland/wallpaperprovider.h
        treeland/wallpaperprovider.cpp
        treeland/wallpaperimage.h
        treeland/wallpaperimage.cpp
        treeland/wallpapercontroller.h
        treeland/wallpapercontroller.cpp
        treeland/wallpapermanager.h
        treeland/wallpapermanager.cpp
        treeland/workspacemodel.h
        treeland/workspacemodel.cpp

        ${DBUS_ADAPTOR}
    QML_FILES
        qml/PrimaryOutput.qml
        qml/CopyOutput.qml
        qml/TitleBar.qml
        qml/Decoration.qml
        qml/TaskBar.qml
        qml/RoundedClipEffect.qml
        qml/SurfaceContent.qml
        qml/Shadow.qml
        qml/Border.qml
        qml/GeometryAnimation.qml
        qml/OutputMenuBar.qml
        qml/WorkspaceSwitcher.qml
        qml/WorkspaceProxy.qml
        qml/Animations/NewAnimation.qml
        qml/TaskSwitcher.qml
        qml/WorkSpaceMask.qml
        qml/SwitchViewDelegate.qml
        qml/SwitchViewHighlightDelegate.qml
        qml/DockPreview.qml

    RESOURCES
        "res/xx.jpg"
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        DDM::Common
        greeterplugin
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickPrivate
        Qt6::DBus
        Waylib::WaylibServer
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
        Dtk6::Core
        Dtk6::Declarative

        foreign-toplevel
        primary-output
        personalization
        shortcut
        wallpaper-color
        window-management
        virtual-output
        dde-shell
        capture
)

target_compile_definitions(${BIN_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

# qt_add_shaders(${BIN_NAME} "treeland_shaders_ng"
#     BATCHABLE
#     PRECOMPILE
#     PREFIX
#         "/shaders"
#     BASE
#         ${PROJECT_RESOURCES_DIR}/shaders
#     FILES
#         ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.vert
#         ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.frag
# )

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
