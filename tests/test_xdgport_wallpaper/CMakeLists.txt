set(CMAKE_AUTOUIC ON)

set(BIN_NAME test-xdg-wallpaper)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(${BIN_NAME}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt6::Widgets
    Qt6::DBus
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
