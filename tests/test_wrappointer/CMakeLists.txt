find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_wrappointer wrapobject.h main.cpp)

target_link_libraries(test_wrappointer
    PRIVATE
        treeland_core
        Qt::Test
)

add_test(NAME test_wrappointer COMMAND test_wrappointer)

set_property(TEST test_wrappointer PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
