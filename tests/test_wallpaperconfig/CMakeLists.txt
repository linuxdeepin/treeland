find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_wallpaperconfig main.cpp)

target_link_libraries(test_wallpaperconfig
    PRIVATE
        libtreeland
        Qt::Test
)

add_test(NAME test_wallpaperconfig COMMAND test_wallpaperconfig)

set_property(TEST test_wallpaperconfig PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_wallpaperconfig PROPERTY
    TIMEOUT 3
)
