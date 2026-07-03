find_package(Qt6 REQUIRED COMPONENTS Test Quick Gui)

add_executable(test_modal_logic main.cpp)

target_link_libraries(test_modal_logic
    PRIVATE
        libtreeland
        Qt::Test
        Qt::Quick
        Qt::Gui
)

add_test(NAME test_modal_logic COMMAND test_modal_logic)

set_property(TEST test_modal_logic PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_modal_logic PROPERTY
    TIMEOUT 5
)
