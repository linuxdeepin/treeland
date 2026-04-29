find_package(Qt6 REQUIRED COMPONENTS Qml Test)

add_executable(test_propertymonitor
    main.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/propertymonitor.h
    ${PROJECT_SOURCE_DIR}/src/utils/propertymonitor.cpp
)

target_include_directories(test_propertymonitor
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(test_propertymonitor
    PRIVATE
        Qt::Qml
        Qt::Test
)

add_test(NAME test_propertymonitor COMMAND test_propertymonitor)

set_property(TEST test_propertymonitor PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_propertymonitor PROPERTY
    TIMEOUT 3
)
