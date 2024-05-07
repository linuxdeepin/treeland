find_package(Qt6 REQUIRED COMPONENTS WaylandClient Widgets)
find_package(PkgConfig)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)

qt_add_library(treeland-shell-manager OBJECT)

qt_generate_wayland_protocol_client_sources(treeland-shell-manager
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-foreign-toplevel-manager-v1.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-shortcut-manager-v1.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-personalization-manager-v1.xml
        ${WAYLAND_PROTOCOLS}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
)

target_link_libraries(treeland-shell-manager
    PUBLIC
    Qt::Core
    PkgConfig::WAYLAND_CLIENT
)

qt_add_executable(treeland-fake-session
    fake-session.h
    fake-session.cpp
    shortcutmanager.h
    shortcutmanager.cpp
)

target_link_libraries(treeland-fake-session
    PRIVATE
        DDM::Common
        $<TARGET_OBJECTS:treeland-shell-manager>
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::DBus
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::WaylandClientPrivate
        PkgConfig::WAYLAND_CLIENT
)

qt_add_executable(treeland-window-bg-test
    wallpaper_test.cpp
)

target_link_libraries(treeland-window-bg-test
    PRIVATE
        DDM::Common
        $<TARGET_OBJECTS:treeland-shell-manager>
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::WaylandClientPrivate
        PkgConfig::WAYLAND_CLIENT
)
install(TARGETS treeland-fake-session RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
