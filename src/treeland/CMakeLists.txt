pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

add_subdirectory(data)
add_subdirectory(protocols)
add_subdirectory(quick)
add_subdirectory(treeland-helper)
add_subdirectory(treeland-fake-session)
add_subdirectory(treeland-wallpaper)

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

qt_add_executable(treeland override_dtkdeclarative_qml.qrc)

target_sources(treeland PUBLIC
FILE_SET HEADERS
FILES
    treeland.h
)

target_sources(treeland PRIVATE
    treeland.cpp
    ${DBUS_INTERFACE}
)

target_link_libraries(treeland
    PRIVATE
        common
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
