find_package(Qt6 REQUIRED COMPONENTS WaylandClient)

qt_add_library(treeland-shell-manager OBJECT)

qt_generate_wayland_protocol_client_sources(treeland-shell-manager
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/ztreeland-foreign-toplevel-manager-v1.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/ztreeland-shortcut-manager-v1.xml
)

target_link_libraries(treeland-shell-manager
    PUBLIC
        Qt::Core
)

qt_add_executable(treeland-fake-session
    fake-session.h
    fake-session.cpp
)

target_link_libraries(treeland-fake-session
    PRIVATE
        common
        $<TARGET_OBJECTS:treeland-shell-manager>
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, ${WaylibServer_LIBRARIES}>
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::DBus
        Qt::WaylandClient
        Qt::GuiPrivate
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
