find_package(TreelandProtocols REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

include(${CMAKE_CURRENT_SOURCE_DIR}/framework/ProtocolTest.cmake)

add_subdirectory(treeland-dde-shell-v1)
