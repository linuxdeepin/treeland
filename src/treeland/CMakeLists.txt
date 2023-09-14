pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

function(ws_generate type input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

    if(NOT EXISTS input_file)
        set(input_file ${CMAKE_CURRENT_SOURCE_DIR}/${input_file})
    endif()

    execute_process(COMMAND ${WAYLAND_SCANNER}
        ${type}-header
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.h
    )

    execute_process(COMMAND ${WAYLAND_SCANNER}
        public-code
        ${input_file}
        ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.c
    )
endfunction()

ws_generate(server data/treeland-wayland-socket-manager.xml socket-server-protocol)
ws_generate(client data/treeland-wayland-socket-manager.xml socket-client-protocol)

ws_generate(server data/treeland-wayland-shortcut-manager.xml shortcut-server-protocol)
ws_generate(client data/treeland-wayland-shortcut-manager.xml shortcut-client-protocol)

set(TREELAND_SOURCES
    treeland.h
    treeland.cpp
    waylandserver.cpp
    shortcut.h
    shortcut.cpp
    waylandsocketproxy.h
    waylandsocketproxy.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/socket-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/socket-server-protocol.c
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-server-protocol.c
)

set(HELPER_SOURCES
    sockethelper.h
    sockethelper.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/socket-client-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/socket-client-protocol.c
)

set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Manager.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)
set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Seat.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)

set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Session.xml" PROPERTIES
   INCLUDE "LogindDBusTypes.h"
)

qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Manager.xml"  "Login1Manager")
qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Seat.xml"  "Login1Seat")
qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.login1.Session.xml"  "Login1Session")

qt_add_executable(treeland
    ${TREELAND_SOURCES}
    ${DBUS_INTERFACE}
)

set_source_files_properties(QmlHelper.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(treeland
    URI TreeLand
    VERSION "1.0"
    SOURCES
        helper.h
        helper.cpp
    QML_FILES
        Main.qml
        CloseAnimation.qml
        OutputDelegate.qml
        QmlHelper.qml
        StackToplevelHelper.qml
        StackWorkspace.qml
        TiledToplevelHelper.qml
        TiledWorkspace.qml
        WindowDecoration.qml
        XdgSurface.qml
    RESOURCE_PREFIX
        /qt/qml
)

target_link_libraries(treeland
    PRIVATE
        common
        greeterplugin
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, ${WaylibServer_LIBRARIES}>
        PkgConfig::WAYLAND_SERVER
        PkgConfig::PIXMAN
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
)

if (NOT WITH_SUBMODULE_WAYLIB)
    target_link_libraries(treeland
        PRIVATE
        QWlroots::QWlroots
    )
    target_include_directories(treeland
        PRIVATE
        ${WaylibServer_INCLUDE_DIR}
    )
endif()

if(JOURNALD_FOUND)
    target_link_libraries(treeland PRIVATE ${JOURNALD_LIBRARIES})
endif()

install(TARGETS treeland RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

add_executable(treeland-helper
    ${HELPER_SOURCES}
)

target_link_libraries(treeland-helper
    PRIVATE
        common
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, ${WaylibServer_LIBRARIES}>
        PkgConfig::WAYLAND_SERVER
        PkgConfig::WAYLAND_CLIENT
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Network
)

if (NOT WITH_SUBMODULE_WAYLIB)
    target_link_libraries(treeland-helper
        PUBLIC
        QWlroots::QWlroots
    )
    target_include_directories(treeland-helper
        PRIVATE
        ${WaylibServer_INCLUDE_DIR}
    )
endif()

install(TARGETS treeland-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

add_executable(treeland-fake-session
    fake-session.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-client-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-client-protocol.c
)

target_link_libraries(treeland-fake-session
    PRIVATE
        common
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, ${WaylibServer_LIBRARIES}>
        PkgConfig::WAYLAND_SERVER
        PkgConfig::WAYLAND_CLIENT
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::DBus
)

if (NOT WITH_SUBMODULE_WAYLIB)
    target_link_libraries(treeland-fake-session
        PRIVATE
        QWlroots::QWlroots
    )
    target_include_directories(treeland-fake-session
        PRIVATE
        ${WaylibServer_INCLUDE_DIR}
    )
endif()

install(TARGETS treeland-fake-session RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
