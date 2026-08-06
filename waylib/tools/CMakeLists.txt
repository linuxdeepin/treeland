project(qtwaylandscanner_waylib CXX)

if (PROJECT_IS_TOP_LEVEL)
    cmake_minimum_required(VERSION 3.16)
    set(CMAKE_CXX_STANDARD 20)
    find_package(Qt6 CONFIG REQUIRED Core)
endif()

if(CMAKE_CROSSCOMPILING AND QTWAYLANDSCANNER_EXECUTABLE)
    add_executable(qtwaylandscanner_waylib IMPORTED GLOBAL)
    set_target_properties(qtwaylandscanner_waylib PROPERTIES IMPORTED_LOCATION ${QTWAYLANDSCANNER_EXECUTABLE})
elseif(CMAKE_CROSSCOMPILING)
    # search native tooling prefix
    set(NATIVE_PREFIX "" CACHE STRING "CMAKE_PREFIX_PATH for native Qt libraries")

    message(STATUS "Building qtwaylandscanner_waylib against ${NATIVE_PREFIX}")

    include(ExternalProject)
    ExternalProject_Add(native_qtwaylandscanner_waylib
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            CMAKE_ARGS -DECM_DIR=${ECM_DIR} -DCMAKE_PREFIX_PATH=${NATIVE_PREFIX}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DQT_MAJOR_VERSION=${QT_MAJOR_VERSION}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_waylib-prefix/src/native_qtwaylandscanner_waylib-build/qtwaylandscanner_waylib
            )
    add_executable(qtwaylandscanner_waylib IMPORTED GLOBAL)
    add_dependencies(qtwaylandscanner_waylib native_qtwaylandscanner_waylib)
    set_target_properties(qtwaylandscanner_waylib PROPERTIES IMPORTED_LOCATION
            ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_waylib-prefix/src/native_qtwaylandscanner_waylib-build/qtwaylandscanner_waylib)
else()
    # host build
    remove_definitions(-DQT_NO_CAST_FROM_ASCII)
    add_executable(qtwaylandscanner_waylib ${CMAKE_CURRENT_SOURCE_DIR}/qtwaylandscanner.cpp)
    target_link_libraries(qtwaylandscanner_waylib Qt6::Core)
endif()

function(waylib_generate_qtwayland_server_protocol target)
    # Parse arguments
    set(options PRIVATE_CODE)
    set(oneValueArgs PROTOCOL BASENAME PREFIX)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to waylib_generate_qtwayland_server_protocol(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()

    set(_prefix "${ARGS_PREFIX}")

    find_package(PkgConfig)
    get_filename_component(_infile ${ARGS_PROTOCOL} ABSOLUTE)
    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    set(server_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-server-protocol.h")
    set(server_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${ARGS_BASENAME}-server-protocol.c")
    execute_process(COMMAND ${WAYLAND_SCANNER} "server-header" ${_infile} ${server_header})
    execute_process(COMMAND ${WAYLAND_SCANNER} "private-code" ${_infile} ${server_code})

    set(_header "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${ARGS_BASENAME}.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${ARGS_BASENAME}.cpp")

    set_source_files_properties(${server_header} ${server_code} ${_header} ${_code} GENERATED)

    add_custom_command(OUTPUT "${_header}"
        COMMAND qtwaylandscanner_waylib server-header ${_infile} "" ${_prefix} > ${_header}
        DEPENDS ${_infile} qtwaylandscanner_waylib VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND qtwaylandscanner_waylib server-code ${_infile} "" ${_prefix} > ${_code}
        DEPENDS ${_infile} ${_header} qtwaylandscanner_waylib VERBATIM)

    set_property(SOURCE ${_header} ${_code} PROPERTY SKIP_AUTOMOC ON)

    target_sources(${target} PRIVATE "${server_code}" "${_code}")

    set(_gen_target generate_protocols_${target}_${ARGS_BASENAME})

    add_custom_target(${_gen_target} ALL
        DEPENDS ${_header} ${_code}
    )

    add_dependencies(${target} ${_gen_target})
endfunction()

