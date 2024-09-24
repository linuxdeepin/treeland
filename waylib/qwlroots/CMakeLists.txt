cmake_minimum_required(VERSION 3.21)

project(QWlroots
    VERSION 0.0.1
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(PREFER_QT_5 "Prefer to use Qt 5 even if we have Qt 6" OFF)
option(USE_WLROOTS_GIT "Use wlroots git version" OFF)
option(QWLROOTS_USE_PERCOMPILE_HEADERS "Use precompile headers to build qwlroots" OFF)
option(DISABLE_WLR_PROTOCOLS "Disable support for wlr protocols, such as wlr-layer-shell" OFF)
option(DISABLE_XWAYLAND "Disable the xwayland support" OFF)

if(PREFER_QT_5)
    find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core)
else()
    find_package(QT NAMES Qt6 QUIET COMPONENTS Core)
    if (NOT QT_FOUND)
        message(FATAL_ERROR "Not found Qt6, set `PREFER_QT_5` option if you want to use Qt5")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(PackageVersionHelper)
include(WaylandScannerHelpers)

# For Unix/Linux
include(GNUInstallDirs)

add_subdirectory(src)
enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)
