find_package(Qt6 REQUIRED COMPONENTS Test Core Network)

# Reuse the extracted helpers from the treeland-debug tool.
set(debug_helpers_dir "${PROJECT_SOURCE_DIR}/tools/treeland-debug")

add_executable(test_treeland_debug
    main.cpp
    "${debug_helpers_dir}/debughelpers.cpp"
)

target_include_directories(test_treeland_debug PRIVATE
    "${debug_helpers_dir}"
    "${PROJECT_SOURCE_DIR}/src/modules/resource"
)

target_link_libraries(test_treeland_debug
    PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Network
)

add_test(NAME test_treeland_debug COMMAND test_treeland_debug)

set_property(TEST test_treeland_debug PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_treeland_debug PROPERTY
    TIMEOUT 10
)
