if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(manual)
endif()
add_subdirectory(unit_tests)
