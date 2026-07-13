find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_wshmtexture
    main.cpp
)

target_link_libraries(test_wshmtexture
    PRIVATE
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Test
)

add_test(NAME test_wshmtexture COMMAND test_wshmtexture)
set_property(TEST test_wshmtexture PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
