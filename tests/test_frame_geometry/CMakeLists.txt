# SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
find_package(Qt6 6.8 REQUIRED COMPONENTS Test Qml Quick)
qt_add_executable(test_frame_geometry
    main.cpp
    "${PROJECT_SOURCE_DIR}/src/surface/framegeometry.cpp"
    "${PROJECT_SOURCE_DIR}/src/surface/framegeometry.h"
    "${PROJECT_SOURCE_DIR}/src/surface/frameinputmask.cpp"
    "${PROJECT_SOURCE_DIR}/src/surface/frameinputmask.h"
)
target_include_directories(test_frame_geometry PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(test_frame_geometry PRIVATE Qt::Test Qt::Qml Qt::Quick Waylib::WaylibServer)
add_test(NAME test_frame_geometry COMMAND test_frame_geometry)
set_tests_properties(test_frame_geometry PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 30
)
