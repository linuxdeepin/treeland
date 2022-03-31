find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Quick QuickControls2 REQUIRED)

set(SOURCES main.cpp)
if(QT_VERSION_MAJOR EQUAL 5)
    qt5_add_resources(SOURCES qml.qrc)
else()
    qt_add_resources(SOURCES qml.qrc)
endif()

add_executable(tinywl
    ${SOURCES}
)

target_link_libraries(tinywl
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    waylibserver
)
