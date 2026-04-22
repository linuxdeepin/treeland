find_package(TreelandProtocols REQUIRED)

local_qtwayland_server_protocol_treeland(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-wine-window-state-unstable-v1.xml
    BASENAME treeland-wine-window-state-v1
)

impl_treeland(
    NAME
        module_wine_window_state
    SOURCE
        winewindowstate.h
        winewindowstate.cpp
        ${CMAKE_BINARY_DIR}/protocols/xdg-shell-protocol.c
        ${CMAKE_BINARY_DIR}/src/modules/wine-window-state/wayland-treeland-wine-window-state-v1-server-protocol.c
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    LINK
        Qt6::Core
)
