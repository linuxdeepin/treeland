cmake_minimum_required(VERSION 3.21)

project(Waylib
    VERSION 0.6.13
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_EXAMPLES "A minimum viable product Wayland compositor based on waylib and other examples" ON)
option(BUILD_TESTS "Build test demos" ON)
option(DISABLE_XWAYLAND "Disable the xwayland support" OFF)
# Don't install tinywl by default, using for debug in local
option(INSTALL_TINYWL "A minimum viable product Wayland compositor based on waylib" OFF)
option(ADDRESS_SANITIZER "Enable address sanitize" OFF)
option(WAYLIB_USE_PERCOMPILE_HEADERS "Use precompile headers to build waylib" OFF)

# When waylib is built standalone (its own top-level project) it should keep
# installing development files by default. When built as a submodule of
# Treeland the parent project already defines TREELAND_INSTALL_DEV=OFF, and
# option() is a no-op for an already-cached variable, so the parent default
# wins. See the Treeland top-level CMakeLists.txt.
option(TREELAND_INSTALL_DEV "Install development files (headers, CMake config, pkg-config, .so symlinks)" ON)

find_package(Qt6 COMPONENTS Core Gui Quick REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QuickPrivate)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/cmake/WaylandScannerHelpers.cmake)

# Standalone build (e.g. waylib CI packaging): the vendored wlroots tree
# lives next to waylib in the monorepo. Pull it in so the Wlroots::wlroots
# target exists instead of requiring an installed libwaylib-wlroots.
if(NOT TARGET Wlroots::wlroots AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../wlroots/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../wlroots"
                     "${CMAKE_CURRENT_BINARY_DIR}/_wlroots")
endif()

if(DISABLE_XWAYLAND)
    add_definitions(-DDISABLE_XWAYLAND)
endif()

if (ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address -fno-optimize-sibling-calls -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# For Unix/Linux
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(PackageVersionHelper)

add_subdirectory(tools)
add_subdirectory(src)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_TESTS)
    enable_testing(true)
    add_subdirectory(tests)
endif()
