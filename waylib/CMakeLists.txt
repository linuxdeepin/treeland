cmake_minimum_required(VERSION 3.21)

project(Waylib
    VERSION 0.6.13
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_WLROOTS_19 "Use wlroots 0.19 version" ON)
option(BUILD_EXAMPLES "A minimum viable product Wayland compositor based on waylib and other examples" ON)
option(BUILD_TESTS "Build test demos" ON)
option(DISABLE_XWAYLAND "Disable the xwayland support" OFF)
option(DISABLE_WLR_PROTOCOLS "Disable support for wlr protocols, such as wlr-layer-shell" OFF)
# Don't install tinywl by default, using for debug in local
option(INSTALL_TINYWL "A minimum viable product Wayland compositor based on waylib" OFF)
option(ADDRESS_SANITIZER "Enable address sanitize" OFF)
option(WAYLIB_USE_PERCOMPILE_HEADERS "Use precompile headers to build waylib" OFF)

find_package(Qt6 COMPONENTS Core Gui Quick REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QuickPrivate)
endif()

# qwlroots infrastructure is now built as part of waylib (no separate library).
# WaylandScannerHelpers provides the ws_generate() macro for protocol code generation.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(WaylandScannerHelpers)
include(PackageVersionHelper)

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

add_subdirectory(src)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_TESTS)
    enable_testing(true)
    add_subdirectory(tests)
endif()
