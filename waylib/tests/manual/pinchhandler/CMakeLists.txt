cmake_minimum_required(VERSION 3.16)

project(pinchhandler VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup()

if(QT_KNOWN_POLICY_QTP0001) # this policy was introduced in Qt 6.5
    qt_policy(SET QTP0001 NEW)
    # the RESOURCE_PREFIX argument for qt_add_qml_module() defaults to ":/qt/qml/"
endif()

qt_add_executable(testPinchhandler
    main.cpp
)

qt_add_qml_module(testPinchhandler
    URI pinchhandler
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES eventitem.cpp
)

target_link_libraries(testPinchhandler
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS testPinchhandler
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
