find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_gestures
    main.cpp
    ${PROJECT_SOURCE_DIR}/src/input/gestures.h
    ${PROJECT_SOURCE_DIR}/src/input/gestures.cpp
)

target_include_directories(test_gestures
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(test_gestures
    PRIVATE
        Qt::Test
)

add_test(NAME test_gestures COMMAND test_gestures)

set_property(TEST test_gestures PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_gestures PROPERTY
    TIMEOUT 3
)
