pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)
find_package(DDM REQUIRED COMPONENTS Common)
# qt_finalize_target will collect all executable's private dependencies that are CMake targets

add_subdirectory(data)
add_subdirectory(protocols)
add_subdirectory(quick)
add_subdirectory(treeland-wallpaper)

qt_add_dbus_adaptor(DBUS_ADAPTOR "${CMAKE_SOURCE_DIR}/data/dbus/org.deepin.compositor1.xml" "treeland.h" TreeLand::TreeLand)

qt_add_executable(treeland override_dtkdeclarative_qml.qrc)

target_sources(treeland PUBLIC
FILE_SET HEADERS
FILES
    treeland.h
)

target_sources(treeland PRIVATE
    treeland.cpp
    ${DBUS_ADAPTOR}
)

target_link_libraries(treeland
    PRIVATE
        DDM::Common
        treeland-quick
        treeland-protocols
        greeterplugin
        Qt${QT_MAJOR_VERSION}::DBus
)

target_compile_definitions(treeland
    PRIVATE
    WLR_USE_UNSTABLE
)

if(JOURNALD_FOUND)
    target_link_libraries(treeland PRIVATE ${JOURNALD_LIBRARIES})
endif()

install(TARGETS treeland RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

## systemd socket

pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml" PROPERTIES
    NO_NAMESPACE ON
    CLASSNAME DisplayManager
)
set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml" PROPERTIES
    NO_NAMESPACE ON
    CLASSNAME DisplaySession
)

qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.xml" DisplayManager)
qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/interfaces/org.freedesktop.DisplayManager.Session.xml" DisplayManagerSession)
qt_add_dbus_interface(DBUS_INTERFACE "${CMAKE_SOURCE_DIR}/data/dbus/org.deepin.compositor1.xml" "DBusCompositor1")

qt_add_executable(treeland-sd
    systemd-socket.cpp
    ${DBUS_INTERFACE}
)

target_link_libraries(treeland-sd
    PRIVATE
        Qt${QT_MAJOR_VERSION}::DBus
        PkgConfig::Systemd
)

install(TARGETS treeland-sd RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
