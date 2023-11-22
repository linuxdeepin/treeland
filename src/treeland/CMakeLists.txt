pkg_search_module(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_search_module(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

function(ws_generate_local type input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
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

function(ws_generate type protocols input_file output_name)
    find_package(PkgConfig)
    pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

    if(NOT EXISTS input_file)
        set(input_file ${WAYLAND_PROTOCOLS}/${input_file})
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

ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/data/ztreeland-shortcut-manager-v1.xml shortcut-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/data/treeland-socket-manager-v1.xml socket-server-protocol)
ws_generate_local(server ${CMAKE_CURRENT_SOURCE_DIR}/data/ztreeland-foreign-toplevel-manager-v1.xml foreign-toplevel-manager-server-protocol)

ws_generate(server wayland-protocols staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml ext-foreign-toplevel-list-server-protocol)

add_subdirectory(treeland-helper)
add_subdirectory(treeland-fake-session)

set(TREELAND_SOURCES
    treeland.h
    treeland.cpp
    socketmanager.h
    socketmanager.cpp
    waylandsocketproxy.h
    waylandsocketproxy.cpp
    treelandhelper.h
    treelandhelper.cpp
    helper.h
    helper.cpp
    foreigntoplevelmanagerv1.h
    foreigntoplevelmanagerv1.cpp
    shortcutmanager.h
    shortcutmanager.cpp
    extforeigntoplevellist.h
    extforeigntoplevellist.cpp
    protocols/foreign_toplevel_manager_impl.cpp
    protocols/shortcut_manager_impl.cpp
    protocols/socket_manager_impl.cpp
    protocols/ext_foreign_toplevel_list_impl.h
    protocols/ext_foreign_toplevel_list_impl.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/socket-server-protocol.c
    ${CMAKE_BINARY_DIR}/src/treeland/socket-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-server-protocol.c
    ${CMAKE_BINARY_DIR}/src/treeland/shortcut-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/foreign-toplevel-manager-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/foreign-toplevel-manager-server-protocol.c
    ${CMAKE_BINARY_DIR}/src/treeland/ext-foreign-toplevel-list-server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/ext-foreign-toplevel-list-server-protocol.c
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

qt_add_executable(treeland)

target_compile_definitions(treeland
    PRIVATE
    WLR_USE_UNSTABLE
)

set_source_files_properties(QmlHelper.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(treeland
    URI TreeLand
    VERSION "1.0"
    SOURCES
        ${TREELAND_SOURCES}
        ${DBUS_INTERFACE}
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
        InputPopupSurface.qml
        LayerSurface.qml
        MiniDock.qml
    RESOURCE_PREFIX
        /qt/qml
)

target_link_libraries(treeland
    PRIVATE
        common
        greeterplugin
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, Waylib::WaylibServer>
        PkgConfig::WAYLAND_SERVER
        PkgConfig::PIXMAN
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
)

if(JOURNALD_FOUND)
    target_link_libraries(treeland PRIVATE ${JOURNALD_LIBRARIES})
endif()

install(TARGETS treeland RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
