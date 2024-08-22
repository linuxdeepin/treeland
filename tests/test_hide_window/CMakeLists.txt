find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(BIN_NAME test-hide-window)

qt_add_executable(${BIN_NAME}
    main.cpp
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Widgets
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
