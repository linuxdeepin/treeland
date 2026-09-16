# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

find_package(Qt6 REQUIRED COMPONENTS WaylandClient Gui Widgets)
find_package(TreelandProtocols REQUIRED)

set(BIN_NAME test-window-appearance)

qt_add_executable(${BIN_NAME}
    main.cpp
    appearance_manager.h
    appearance_manager.cpp
)

qt_generate_wayland_protocol_client_sources(${BIN_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-unstable-v1.xml
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-appearance-manager-unstable-v1.xml
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        Qt6::Gui
        Qt6::Widgets
        Qt6::WaylandClient
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
