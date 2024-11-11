find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_protocol_wallpaper-color main.cpp)

target_link_libraries(test_protocol_wallpaper-color
    PRIVATE
        Waylib::WaylibServer
        wallpaper-color
        Qt::Test
)

add_test(NAME test_protocol_wallpaper-color COMMAND test_protocol_wallpaper-color)

set_property(TEST test_protocol_wallpaper-color PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
