find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Network RemoteObjects)

# Reuse the extracted helpers from the treeland-debug tool.
set(debug_helpers_dir "${PROJECT_SOURCE_DIR}/tools/treeland-debug")

# debugsession.cpp serialises the replica POD structs, so the test needs the
# same replica header the tool builds. Nothing here connects to a compositor:
# the JSON and target-resolution helpers are pure logic.
#
# This spells out repc instead of using qt_add_repc_replicas() because that
# helper derives the header name from the .rep file name
# (rep_treelandwindowtree_replica.h here), while debugsession.h includes the
# name the tool's build generates: rep_treeland_windowtree_replica.h.
set(windowtree_rep_file
    "${PROJECT_SOURCE_DIR}/src/modules/resource/treelandwindowtree.rep"
)
set(windowtree_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(windowtree_replica_header
    "${windowtree_generated_dir}/rep_treeland_windowtree_replica.h"
)

add_custom_command(
    OUTPUT "${windowtree_replica_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${windowtree_generated_dir}"
    COMMAND "$<TARGET_FILE:Qt6::repc>"
        -i rep
        -o replica
        "${windowtree_rep_file}"
        "${windowtree_replica_header}"
    DEPENDS "${windowtree_rep_file}"
    VERBATIM
)

add_executable(test_treeland_debug
    main.cpp
    "${debug_helpers_dir}/debughelpers.cpp"
    "${debug_helpers_dir}/debugsession.cpp"
    "${windowtree_replica_header}"
    "${PROJECT_SOURCE_DIR}/src/modules/resource/treelanddebugsocket.cpp"
)

target_include_directories(test_treeland_debug PRIVATE
    "${debug_helpers_dir}"
    "${windowtree_generated_dir}"
    "${PROJECT_SOURCE_DIR}/src/modules/resource"
)

target_link_libraries(test_treeland_debug
    PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::RemoteObjects
)

add_test(NAME test_treeland_debug COMMAND test_treeland_debug)

set_property(TEST test_treeland_debug PROPERTY
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_property(TEST test_treeland_debug PROPERTY
    TIMEOUT 10
)
