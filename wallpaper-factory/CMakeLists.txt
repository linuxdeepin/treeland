set(BIN_NAME treeland-wallpaper-factory)

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS WaylandClient Quick)
find_package(Qt6 COMPONENTS WaylandClientPrivate QuickPrivate QUIET)
find_package(TreelandProtocols REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt6_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wallpaper-shell-unstable-v1.xml
)

qt_add_qml_module(${BIN_NAME}
    URI com.treeland.wallfactory
    VERSION 1.0
    SOURCES
    loggings.h
    loggings.cpp
    mpvvideoitem.h
    mpvvideoitem.cpp
    mpvvideocontroller.h
    mpvvideocontroller.cpp
    qwaylandwallpapershellintegration_p.h
    qwaylandwallpapershellintegration.cpp
    qwaylandwallpapershellintegrationplugin.cpp
    qwaylandwallpapersurface_p.h
    qwaylandwallpapersurface.cpp
    wallpaperwindow.h
    wallpaperwindow.cpp
    treelandwallpapernotifierclient.h
    treelandwallpapernotifierclient.cpp
    QML_FILES
    Image.qml
    Video.qml

)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Quick
        Qt6::QuickPrivate
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        PkgConfig::MPV
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
