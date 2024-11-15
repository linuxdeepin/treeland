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
find_package(Qt6 CONFIG REQUIRED ShaderTools Concurrent)
find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(LIBINPUT REQUIRED IMPORTED_TARGET libinput)
# qt_finalize_target will collect all executable's private dependencies that are CMake targets

add_subdirectory(modules)
add_subdirectory(greeter)
add_subdirectory(treeland-wallpaper)
add_subdirectory(treeland-shortcut)

qt_add_dbus_adaptor(DBUS_ADAPTOR "${CMAKE_SOURCE_DIR}/misc/dbus/org.deepin.compositor1.xml" "treeland.h" Treeland::Treeland)

qt_add_library(config
    STATIC
)

target_sources(config PUBLIC
FILE_SET HEADERS
FILES
    core/treelandconfig.h
)

target_include_directories(config PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/>
)

target_sources(config PRIVATE
    core/treelandconfig.cpp
)

target_link_libraries(config
    PUBLIC
        Dtk6::Core
        Qt6::Quick
)

set(BIN_NAME treeland)

qt_add_executable(${BIN_NAME}
    main.cpp
    # ${DBUS_ADAPTOR}
)

qt_add_resources(${BIN_NAME} "treeland_assets"
    PREFIX "/dsg/icons"
    BASE ${PROJECT_RESOURCES_DIR}/icons
    FILES
        ${PROJECT_RESOURCES_DIR}/icons/multitaskview_close.dci
        ${PROJECT_RESOURCES_DIR}/icons/close.dci
        ${PROJECT_RESOURCES_DIR}/icons/add.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_output.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_window.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_region.dci
)

target_include_directories(${BIN_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/treeland
)

qt_add_qml_module(${BIN_NAME}
    URI Treeland
    VERSION "2.0"

    SOURCES
        core/cmdline.h
        core/cmdline.cpp
        core/helper.h
        core/helper.cpp
        core/surfacewrapper.h
        core/surfacewrapper.cpp
        core/workspace.h
        core/workspace.cpp
        core/shellhandler.h
        core/shellhandler.cpp
        core/output.h
        core/output.cpp
        core/qmlengine.h
        core/qmlengine.cpp
        core/surfacecontainer.h
        core/surfacecontainer.cpp
        core/layersurfacecontainer.h
        core/layersurfacecontainer.cpp
        core/lockscreen.h
        core/lockscreen.cpp
        core/rootsurfacecontainer.h
        core/rootsurfacecontainer.cpp
        core/surfaceproxy.h
        core/surfaceproxy.cpp
        core/treeland.h
        core/treeland.cpp
        core/wallpaperprovider.h
        core/wallpaperprovider.cpp
        core/wallpaperimage.h
        core/wallpaperimage.cpp
        core/wallpapercontroller.h
        core/wallpapercontroller.cpp
        core/wallpapermanager.h
        core/wallpapermanager.cpp
        core/workspacemodel.h
        core/workspacemodel.cpp
        core/multitaskview.h
        core/multitaskview.cpp
        core/propertymonitor.h
        core/propertymonitor.cpp
        core/tsgradiusimagenode.h
        core/tsgradiusimagenode.cpp
        core/tquickradiuseffect_p.h
        core/tquickradiuseffect.h
        core/tquickradiuseffect.cpp
        core/gestures.h
        core/gestures.cpp
        core/togglablegesture.h
        core/togglablegesture.cpp
        core/inputdevice.h
        core/inputdevice.cpp
        core/workspaceanimationcontroller.h
        core/workspaceanimationcontroller.cpp
        core/surfacefilterproxymodel.h
        core/surfacefilterproxymodel.cpp
        core/windowpicker.h
        core/windowpicker.cpp

        ${DBUS_ADAPTOR}
    QML_FILES
        qml/PrimaryOutput.qml
        qml/CopyOutput.qml
        qml/TitleBar.qml
        qml/Decoration.qml
        qml/WindowMenu.qml
        qml/TaskBar.qml
        qml/SurfaceContent.qml
        qml/XdgShadow.qml
        qml/Border.qml
        qml/OutputMenuBar.qml
        qml/WorkspaceSwitcher.qml
        qml/WorkspaceProxy.qml
        qml/Animations/GeometryAnimation.qml
        qml/Animations/NewAnimation.qml
        qml/Animations/MinimizeAnimation.qml
        qml/Animations/ShowDesktopAnimation.qml
        qml/Animations/LaunchpadAnimation.qml
        qml/Animations/LayerShellAnimation.qml
        qml/Effects/Blur.qml
        qml/Effects/LaunchpadCover.qml
        qml/TaskSwitcher.qml
        qml/TaskWindowPreview.qml
        qml/SwitchViewDelegate.qml
        qml/SwitchViewHighlightDelegate.qml
        qml/DockPreview.qml
        qml/MultitaskviewProxy.qml
        qml/WindowSelectionGrid.qml
        qml/WorkspaceSelectionList.qml
        qml/FadeBehavior.qml
        qml/CaptureSelectorLayer.qml
        qml/WindowPickerLayer.qml
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        DDM::Common
        greeterplugin
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickPrivate
        Qt6::DBus
        Qt6::Concurrent
        Waylib::WaylibServer
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
        PkgConfig::LIBINPUT
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
        captureplugin
        config
)

target_compile_definitions(${BIN_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

qt_add_shaders(${BIN_NAME} "treeland_shaders_ng"
    BATCHABLE
    PRECOMPILE
    PREFIX
        "/shaders"
    BASE
        ${PROJECT_RESOURCES_DIR}/shaders
    FILES
        ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.vert
        ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.frag
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
