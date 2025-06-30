find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS WaylandClient Quick Core Gui)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)
find_package(TreelandProtocols REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)

# Include translation utilities
include(${CMAKE_SOURCE_DIR}/cmake/TranslationUtils.cmake)

set(BIN_NAME test-set-treeland-wallpaper)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-personalization-manager-v1.xml
)

set(QML_FILES
    main.qml
    Wallpaper.qml
    WallpaperCard.qml
    WallpaperSetting.qml
    PropertyItemDelegate.qml
)

qt_add_qml_module(${BIN_NAME}
    URI Wallpaper
    VERSION "1.0"
    SOURCES
    personalizationmangerclient.cpp
    personalizationmangerclient.h
    wallpapercardmodel.cpp
    wallpapercardmodel.h
    QML_FILES ${QML_FILES}
    SOURCES imagehelper.h imagehelper.cpp
)

# Use translation utilities to setup translations
setup_translations(${BIN_NAME} wallpaper)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::WaylandClient
        Dtk6::Declarative
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
