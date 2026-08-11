# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

treeland_add_desktop_integration_test(
    NAME rendered_output_fixture
    SETUP "${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp"
    CLIENT "${CMAKE_CURRENT_SOURCE_DIR}/rendered-output-fixture.c"
)
