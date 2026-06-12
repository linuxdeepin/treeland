find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)

set(BIN_NAME test-im-candidate-panel-xprop)

qt_add_executable(${BIN_NAME}
    main.cpp
)

target_include_directories(${BIN_NAME} PRIVATE ${XCB_INCLUDE_DIRS})
target_link_libraries(${BIN_NAME} PRIVATE Qt6::Widgets ${XCB_LIBRARIES})

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
