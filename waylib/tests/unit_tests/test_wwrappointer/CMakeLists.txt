find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_wwrappointer wrapobject.h main.cpp)

target_link_libraries(test_wwrappointer
    PRIVATE
        Waylib::WaylibServer
        Qt::Test
)

add_test(NAME test_wwrappointer COMMAND test_wwrappointer)

set_property(TEST test_wwrappointer PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
