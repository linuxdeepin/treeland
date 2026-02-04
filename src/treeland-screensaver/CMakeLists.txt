pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_package(TreelandProtocols REQUIRED)

set(TREELAND_SCREENSAVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/treeland-screensaver-v1.c)
set(TREELAND_SCREENSAVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/treeland-screensaver-v1.h)

add_custom_command(
    OUTPUT ${TREELAND_SCREENSAVER_HEADER}
    COMMAND wayland-scanner client-header < ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml > ${TREELAND_SCREENSAVER_HEADER}
    DEPENDS ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml
    COMMENT "Generating treeland-screensaver-v1.h"
)

add_custom_command(
    OUTPUT ${TREELAND_SCREENSAVER_SOURCE}
    COMMAND wayland-scanner private-code < ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml > ${TREELAND_SCREENSAVER_SOURCE}
    DEPENDS ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-screensaver-v1.xml
    COMMENT "Generating treeland-screensaver-v1.c"
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
