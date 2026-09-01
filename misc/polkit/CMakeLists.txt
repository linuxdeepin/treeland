# Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/org.deepin.dde.treeland-debug.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.dde.treeland-debug.policy
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.dde.treeland-debug.policy
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions"
)
