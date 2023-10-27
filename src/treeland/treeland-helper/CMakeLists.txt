find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient)
find_package(XKB REQUIRED)

set(HELPER_SOURCES
    sockethelper.h
    sockethelper.cpp
)

qt_add_library(treeland-socket-manager OBJECT)
qt_generate_wayland_protocol_client_sources(treeland-socket-manager
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-socket-manager-v1.xml
)
target_link_libraries(treeland-socket-manager
    PUBLIC
        Qt::Core
)

qt_add_executable(treeland-helper
    ${HELPER_SOURCES}
)

target_link_libraries(treeland-helper
    PRIVATE
        common
        $<TARGET_OBJECTS:treeland-socket-manager>
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, ${WaylibServer_LIBRARIES}>
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::WaylandClient
        Qt::GuiPrivate
        Qt::CorePrivate
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
