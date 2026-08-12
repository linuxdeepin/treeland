find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(test_containerof main.cpp)

target_link_libraries(test_containerof
    PRIVATE
        Qt::Core
        Qt::Test
)

target_include_directories(test_containerof PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/utils"
)

add_test(NAME test_containerof COMMAND test_containerof)

# Negative compile check: container_of() must reject non-standard-layout
# container types via static_assert. negative_compile.cpp instantiates it
# with a std::string member, so a successful build means the guard regressed.
set(_containerof_inc "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/server/utils")
try_compile(_containerof_negative_compiled
    "${CMAKE_CURRENT_BINARY_DIR}/containerof_negative"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/negative_compile.cpp"
    CMAKE_FLAGS
        "-DCMAKE_CXX_STANDARD=20"
        "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
        "-DCMAKE_CXX_FLAGS=-I${_containerof_inc}")
if(_containerof_negative_compiled)
    message(FATAL_ERROR
        "W_CONTAINER_OF must reject non-standard-layout types, but "
        "negative_compile.cpp compiled successfully")
endif()
