include(CMakeParseArguments)

function(impl_treeland)
    set(one_value_args NAME)
    set(multi_value_args SOURCE INCLUDE LINK)

    cmake_parse_arguments(
        PARSE_ARG_PREFIX
        ""
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT PARSE_ARG_PREFIX_NAME)
        message(FATAL_ERROR "NAME is a required argument!")
    endif()

    add_library(${PARSE_ARG_PREFIX_NAME} INTERFACE)

    target_sources(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_SOURCE}
    )

    target_include_directories(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_INCLUDE}
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    )

    set(PRIVATE_LIBS
        # TODO: remove this
        Dtk6::Core
        Dtk6::Declarative
        Dtk6::SystemSettings
        Waylib::WaylibServer
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::DBus
        Qt6::Concurrent
        PkgConfig::PIXMAN
        PkgConfig::WAYLAND
        PkgConfig::LIBINPUT
        # TODO: end remove
        )

    # Conditionally link Qt6::QuickPrivate if available
    if(QT6_QUICKPRIVATE_FOUND)
        list(APPEND PRIVATE_LIBS Qt6::QuickPrivate)
    endif()

    target_link_libraries(${PARSE_ARG_PREFIX_NAME}
        INTERFACE
            ${PARSE_ARG_PREFIX_LINK}
            ${PRIVATE_LIBS}
    )

    target_link_libraries(libtreeland
        PRIVATE
            ${PARSE_ARG_PREFIX_NAME}
    )
endfunction()

