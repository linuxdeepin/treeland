find_package(Qt6 COMPONENTS Widgets Quick QuickControls2 REQUIRED)

set(SOURCES main.cpp)
qt_add_resources(SOURCES qml.qrc)

add_executable(tinywl-qtquick
    ${SOURCES}
)

target_link_libraries(tinywl-qtquick
    PRIVATE
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    waylibserver
)

install(TARGETS tinywl-qtquick DESTINATION ${CMAKE_INSTALL_BINDIR})
