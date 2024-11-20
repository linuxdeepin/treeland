find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)

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
        ${PROJECT_RESOURCES_DIR}/icons/close.dci
        ${PROJECT_RESOURCES_DIR}/icons/add.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_output.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_window.dci
        ${PROJECT_RESOURCES_DIR}/icons/select_region.dci
)

target_include_directories(treeland_core
    PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/treeland>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

set(CORE_HEADERS
    cmdline.h
    helper.h
    surfacewrapper.h
    workspace.h
    shellhandler.h
    output.h
    qmlengine.h
    surfacecontainer.h
    layersurfacecontainer.h
    lockscreen.h
    rootsurfacecontainer.h
    surfaceproxy.h
    treeland.h
    treeland.cpp
    wallpaperprovider.h
    wallpaperimage.h
    wallpapercontroller.h
    wallpapermanager.h
    workspacemodel.h
    propertymonitor.h
    tsgradiusimagenode.h
    tquickradiuseffect_p.h
    tquickradiuseffect.h
    gestures.h
    togglablegesture.h
    inputdevice.h
    workspaceanimationcontroller.h
    surfacefilterproxymodel.h
    windowpicker.h
)

set(CORE_SRCS
    cmdline.cpp
    helper.cpp
    surfacewrapper.cpp
    workspace.cpp
    shellhandler.cpp
    output.cpp
    qmlengine.cpp
    surfacecontainer.cpp
    layersurfacecontainer.cpp
    lockscreen.cpp
    rootsurfacecontainer.cpp
    surfaceproxy.cpp
    treeland.cpp
    wallpaperprovider.cpp
    wallpaperimage.cpp
    wallpapercontroller.cpp
    wallpapermanager.cpp
    workspacemodel.cpp
    propertymonitor.cpp
    tsgradiusimagenode.cpp
    tquickradiuseffect.cpp
    gestures.cpp
    togglablegesture.cpp
    inputdevice.cpp
    workspaceanimationcontroller.cpp
    surfacefilterproxymodel.cpp
    windowpicker.cpp
)

qt_add_qml_module(treeland_core
    URI Treeland
    VERSION "2.0"

    SOURCES
        ${CORE_HEADERS}
        ${CORE_SRCS}
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
        treeland_interface
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
)

set(TRANSLATED_FILES)
if (${QT_VERSION_MINOR} LESS 7)
    qt_add_lupdate(treeland_core
        TS_FILES
            ${CMAKE_SOURCE_DIR}/translations/treeland.zh_CN.ts
            ${CMAKE_SOURCE_DIR}/translations/treeland.en_US.ts
        NO_GLOBAL_TARGET
    )

    qt_add_lrelease(treeland_core
        TS_FILES
            ${CMAKE_SOURCE_DIR}/translations/treeland.zh_CN.ts
            ${CMAKE_SOURCE_DIR}/translations/treeland.en_US.ts
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
elseif (${QT_VERSION_MINOR} GREATER_EQUAL 7)
    qt_add_lupdate(
        SOURCE_TARGETS treeland_core
        TS_FILES
            ${CMAKE_SOURCE_DIR}/translations/treeland.zh_CN.ts
            ${CMAKE_SOURCE_DIR}/translations/treeland.en_US.ts
        NO_GLOBAL_TARGET
    )

    qt_add_lrelease(
        TS_FILES
            ${CMAKE_SOURCE_DIR}/translations/treeland.zh_CN.ts
            ${CMAKE_SOURCE_DIR}/translations/treeland.en_US.ts
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
endif()

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

install(FILES ${TRANSLATED_FILES} DESTINATION ${TREELAND_COMPONENTS_TRANSLATION_DIR})

install(FILES
    ${CORE_HEADERS}
    DESTINATION include/treeland
)

set_target_properties(treeland_core PROPERTIES
    EXPORT_NAME Core
)

install(
    TARGETS
        treeland_core
    # EXPORT TreelandTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install(EXPORT TreelandTargets
#     FILE TreelandTargets.cmake
#     NAMESPACE Treeland::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/treeland
# )
