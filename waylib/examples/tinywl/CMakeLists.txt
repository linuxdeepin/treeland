find_package(Qt5 COMPONENTS Quick QuickControls2 REQUIRED)

set(SOURCES main.cpp)
qt5_add_resources(SOURCES qml.qrc)

add_executable(tinywl
    ${SOURCES}
)

target_link_libraries(tinywl
    PRIVATE
    Qt5::Quick
    Qt5::QuickControls2
    waylibserver
)
