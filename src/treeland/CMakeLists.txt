include_directories(
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/src/common"
)

add_definitions(-DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

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

ws_generate(server data/treeland-wayland-socket-manager.xml server-protocol)
ws_generate(client data/treeland-wayland-socket-manager.xml client-protocol)

set(TREELAND_SOURCES
    treeland.h
    treeland.cpp
    helper.h
    helper.cpp
    waylandserver.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/server-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/server-protocol.c
)

set(HELPER_SOURCES
    sockethelper.h
    sockethelper.cpp
)

add_executable(treeland
    ${TREELAND_SOURCES}
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
    QML_FILES
        Main.qml
        ClientWindow.qml
        XdgStackWorkspace.qml
        XdgSurface.qml
        XdgTiledWorkspace.qml
        QmlHelper.qml
        OutputDelegate.qml
)

target_link_libraries(treeland
    PRIVATE
        waylibserver
        PkgConfig::WAYLAND_SERVER
        PkgConfig::PIXMAN
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
)

if(JOURNALD_FOUND)
    target_link_libraries(treeland PRIVATE ${JOURNALD_LIBRARIES})
endif()

install(TARGETS treeland RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


add_executable(treeland-helper
    ${HELPER_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_BINARY_DIR}/src/treeland/client-protocol.h
    ${CMAKE_BINARY_DIR}/src/treeland/client-protocol.c
)

target_link_libraries(treeland-helper
    PRIVATE
        waylibserver
        PkgConfig::WAYLAND_SERVER
        PkgConfig::WAYLAND_CLIENT
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Network
)

install(TARGETS treeland-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
