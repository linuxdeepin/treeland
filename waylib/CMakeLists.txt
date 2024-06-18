cmake_minimum_required(VERSION 3.21)

project(Waylib
    VERSION 0.1.1
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_SUBMODULE_QWLROOTS "Use the QWlroots from git submodule" OFF)
option(BUILD_TINYWL "A minimum viable product Wayland compositor based on waylib" ON)
option(DISABLE_XWAYLAND "Disable the xwayland support" OFF)
# Don't install tinywl by default, using for debug in local
option(INSTALL_TINYWL "A minimum viable product Wayland compositor based on waylib" OFF)
option(ADDRESS_SANITIZER "Enable address sanitize" OFF)

if(WITH_SUBMODULE_QWLROOTS)
    add_subdirectory(qwlroots)
    message("Using QWlroots from submodule")
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
include(WaylandScannerHelpers)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
