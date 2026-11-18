
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/treeland-prelaunch-splash-unstable-v2.xml
    BASENAME treeland-prelaunch-splash-unstable-v2
)

impl_treeland(
    NAME
        module_prelaunch_splash
    SOURCE
        prelaunchsplash.h
        prelaunchsplash.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)
