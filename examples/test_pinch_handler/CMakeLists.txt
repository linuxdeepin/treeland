set(BIN_NAME test-pinch-handler)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_add_executable(${BIN_NAME}
    main.cpp
)

qt_add_qml_module(${BIN_NAME}
    URI pinchhandler
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES eventitem.cpp
)

target_link_libraries(${BIN_NAME}
    PRIVATE Qt6::Quick
)

install(TARGETS ${BIN_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
