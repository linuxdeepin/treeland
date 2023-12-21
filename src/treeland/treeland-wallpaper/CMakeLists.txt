find_package(Qt6 REQUIRED COMPONENTS WaylandClient Quick Core Gui)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)
find_package(PkgConfig)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ CACHE STRING "" FORCE)

qt_add_executable(treeland-wallpaper
    main.cpp
)

qt_generate_wayland_protocol_client_sources(treeland-wallpaper
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-personalization-manager-v1.xml
)

qt_add_qml_module(treeland-wallpaper
    URI Wallpaper
    VERSION "1.0"
    SOURCES
        personalizationmangerclient.cpp
    QML_FILES
        main.qml
        Wallpaper.qml
        PropertyItemDelegate.qml
)

target_link_libraries(treeland-wallpaper
    PRIVATE
        common
        Qt${QT_MAJOR_VERSION}::Gui
        Qt::WaylandClient
        Dtk6::Declarative
)

install(TARGETS treeland-wallpaper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
