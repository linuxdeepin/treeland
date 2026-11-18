pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

set(TREELAND_SCREENSAVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/treeland-screensaver-v2.c)
set(TREELAND_SCREENSAVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/treeland-screensaver-v2.h)

add_custom_command(
    OUTPUT ${TREELAND_SCREENSAVER_HEADER}
    COMMAND wayland-scanner client-header < ${PROJECT_SOURCE_DIR}/src/protocols/treeland-screensaver-unstable-v2.xml > ${TREELAND_SCREENSAVER_HEADER}
    DEPENDS ${PROJECT_SOURCE_DIR}/src/protocols/treeland-screensaver-unstable-v2.xml
    COMMENT "Generating treeland-screensaver-v2.h"
)

add_custom_command(
    OUTPUT ${TREELAND_SCREENSAVER_SOURCE}
    COMMAND wayland-scanner private-code < ${PROJECT_SOURCE_DIR}/src/protocols/treeland-screensaver-unstable-v2.xml > ${TREELAND_SCREENSAVER_SOURCE}
    DEPENDS ${PROJECT_SOURCE_DIR}/src/protocols/treeland-screensaver-unstable-v2.xml
    COMMENT "Generating treeland-screensaver-v2.c"
)

qt_add_executable(treeland-screensaver
    screensaver.cpp
    ${TREELAND_SCREENSAVER_HEADER}
    ${TREELAND_SCREENSAVER_SOURCE}
)

target_link_libraries(treeland-screensaver
    PUBLIC
        Qt6::DBus
        PkgConfig::WAYLAND_CLIENT
)

install(TARGETS treeland-screensaver RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(FILES org.freedesktop.ScreenSaver.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces")
