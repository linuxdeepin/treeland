if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    cmake_minimum_required(VERSION 3.16)
    project(qtwaylandscanner_treeland CXX)
    set(CMAKE_CXX_STANDARD 20)
    find_package(Qt6 CONFIG REQUIRED Core)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

if(CMAKE_CROSSCOMPILING AND QTWAYLANDSCANNER_TREELAND_EXECUTABLE)
    add_executable(qtwaylandscanner_treeland IMPORTED GLOBAL)
    set_target_properties(qtwaylandscanner_treeland PROPERTIES IMPORTED_LOCATION ${QTWAYLANDSCANNER_TREELAND_EXECUTABLE})
elseif(CMAKE_CROSSCOMPILING)
    # search native tooling prefix
    set(NATIVE_PREFIX "" CACHE STRING "CMAKE_PREFIX_PATH for native Qt libraries")
    if (NOT NATIVE_PREFIX)
        string(SUBSTRING ${idx} NATIVE_PREFIX)
    endif()

    message(STATUS "Building qtwaylandscanner_treeland against ${NATIVE_PREFIX}")

    include(ExternalProject)
    ExternalProject_Add(native_qtwaylandscanner_treeland
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            CMAKE_ARGS -DECM_DIR=${ECM_DIR} -DCMAKE_PREFIX_PATH=${NATIVE_PREFIX}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DQT_MAJOR_VERSION=${QT_MAJOR_VERSION}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_treeland-prefix/src/native_qtwaylandscanner_treeland-build/qtwaylandscanner_treeland
            )
    add_executable(qtwaylandscanner_treeland IMPORTED GLOBAL)
    add_dependencies(qtwaylandscanner_treeland native_qtwaylandscanner_treeland)
    set_target_properties(qtwaylandscanner_treeland PROPERTIES IMPORTED_LOCATION
            ${CMAKE_CURRENT_BINARY_DIR}/native_qtwaylandscanner_treeland-prefix/src/native_qtwaylandscanner_treeland-build/qtwaylandscanner_treeland)
else()
    # host build
    remove_definitions(-DQT_NO_CAST_FROM_ASCII)
    add_executable(qtwaylandscanner_treeland qtwaylandscanner.cpp)
    target_link_libraries(qtwaylandscanner_treeland Qt::Core)
endif()

function(local_qtwayland_server_protocol_treeland target)
    # Parse arguments
    set(options PRIVATE_CODE)
    set(oneValueArgs PROTOCOL BASENAME PREFIX)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to local_qtwayland_server_protocol_treeland(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
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

    set_source_files_properties(${_header} ${_code} GENERATED)

    add_custom_command(OUTPUT "${_header}"
        COMMAND qtwaylandscanner_treeland server-header ${_infile} "" ${_prefix} > ${_header}
        DEPENDS ${_infile} qtwaylandscanner_treeland VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND qtwaylandscanner_treeland server-code ${_infile} "" ${_prefix} > ${_code}
        DEPENDS ${_infile} ${_header} qtwaylandscanner_treeland VERBATIM)

    set_property(SOURCE ${_header} ${_code} PROPERTY SKIP_AUTOMOC ON)

    target_sources(${target} PRIVATE "${_code}")
endfunction()
