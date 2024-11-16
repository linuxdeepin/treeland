qt_add_dbus_adaptor(DBUS_ADAPTOR "${CMAKE_SOURCE_DIR}/misc/dbus/org.deepin.compositor1.xml" "treeland.h" Treeland::Treeland)

qt_add_library(config INTERFACE)

target_sources(config INTERFACE
    treelandconfig.h
    treelandconfig.cpp
)

target_include_directories(config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_link_libraries(config
    INTERFACE
        Dtk6::Core
        Qt6::Quick
)

add_library(treeland_core SHARED)

qt_add_resources(treeland_core "treeland_assets"
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

target_include_directories(treeland_core
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

qt_add_qml_module(treeland_core
    URI Treeland
    VERSION "2.0"

    SOURCES
        cmdline.h
        cmdline.cpp
        helper.h
        helper.cpp
        surfacewrapper.h
        surfacewrapper.cpp
        workspace.h
        workspace.cpp
        shellhandler.h
        shellhandler.cpp
        output.h
        output.cpp
        qmlengine.h
        qmlengine.cpp
        surfacecontainer.h
        surfacecontainer.cpp
        layersurfacecontainer.h
        layersurfacecontainer.cpp
        lockscreen.h
        lockscreen.cpp
        rootsurfacecontainer.h
        rootsurfacecontainer.cpp
        surfaceproxy.h
        surfaceproxy.cpp
        treeland.h
        treeland.cpp
        wallpaperprovider.h
        wallpaperprovider.cpp
        wallpaperimage.h
        wallpaperimage.cpp
        wallpapercontroller.h
        wallpapercontroller.cpp
        wallpapermanager.h
        wallpapermanager.cpp
        workspacemodel.h
        workspacemodel.cpp
        propertymonitor.h
        propertymonitor.cpp
        tsgradiusimagenode.h
        tsgradiusimagenode.cpp
        tquickradiuseffect_p.h
        tquickradiuseffect.h
        tquickradiuseffect.cpp
        gestures.h
        gestures.cpp
        togglablegesture.h
        togglablegesture.cpp
        inputdevice.h
        inputdevice.cpp
        workspaceanimationcontroller.h
        workspaceanimationcontroller.cpp
        surfacefilterproxymodel.h
        surfacefilterproxymodel.cpp
        windowpicker.h
        windowpicker.cpp

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

target_link_libraries(treeland_core
    PUBLIC
        Dtk6::Core
        Dtk6::Declarative
        Waylib::WaylibServer
        foreign-toplevel
        primary-output
        personalization
        shortcut
        wallpaper-color
        window-management
        virtual-output
        dde-shell
        capture
        config
        greeter
    PRIVATE
        DDM::Common
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickPrivate
        Qt6::DBus
        Qt6::Concurrent
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
        PkgConfig::LIBINPUT
        treeland_interface
)

target_compile_definitions(treeland_core
    PRIVATE
    WLR_USE_UNSTABLE
)

qt_add_shaders(treeland_core "treeland_shaders_ng"
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

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gestures.h
    ${CMAKE_CURRENT_SOURCE_DIR}/helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inputdevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/layersurfacecontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lockscreen.h
    ${CMAKE_CURRENT_SOURCE_DIR}/output.h
    ${CMAKE_CURRENT_SOURCE_DIR}/propertymonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/qmlengine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rootsurfacecontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shellhandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shellhandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surfacecontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surfacefilterproxymodel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surfaceproxy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surfacewrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/togglablegesture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tquickradiuseffect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tquickradiuseffect_p.h
    ${CMAKE_CURRENT_SOURCE_DIR}/treelandconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/treeland.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tsgradiusimagenode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wallpapercontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wallpaperimage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wallpapermanager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wallpaperprovider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/windowpicker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workspaceanimationcontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workspace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workspacemodel.h
    DESTINATION include/treeland
)

set_target_properties(treeland_core PROPERTIES
    EXPORT_NAME Core
)

install(
    TARGETS
        treeland_core
    EXPORT TreelandTargets
    LIBRARY DESTINATION lib
)

# install(EXPORT TreelandTargets
#     FILE TreelandTargets.cmake
#     NAMESPACE Treeland::
#     DESTINATION lib/cmake/treeland
# )