cmake_minimum_required(VERSION 3.1.0)

project(Waylib
    VERSION 0.0.1
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_SUBMODULE_QWLROOTS "Use the QWlroots from git submodule" OFF)
option(BUILD_TINYWL "A minimum viable product Wayland compositor based on waylib" ON)
option(DISABLE_XWAYLAND "Disable the xwayland support" OFF)

if(WITH_SUBMODULE_QWLROOTS)
    add_subdirectory(qwlroots)
    message("Using QWlroots from submodule")
endif()

if(DISABLE_XWAYLAND)
    add_definitions(-DDISABLE_XWAYLAND)
endif()

# For Unix/Linux
include(GNUInstallDirs)

macro (setup_package_version_variables _packageName)
    if (DEFINED ${_packageName}_VERSION)
        string (REGEX MATCHALL "[0-9]+" _versionComponents "${${_packageName}_VERSION}")
        list (LENGTH _versionComponents _len)
        if (${_len} GREATER 0)
            list(GET _versionComponents 0 ${_packageName}_VERSION_MAJOR)
        endif()
        if (${_len} GREATER 1)
            list(GET _versionComponents 1 ${_packageName}_VERSION_MINOR)
        endif()
        if (${_len} GREATER 2)
            list(GET _versionComponents 2 ${_packageName}_VERSION_PATCH)
        endif()
        if (${_len} GREATER 3)
            list(GET _versionComponents 3 ${_packageName}_VERSION_TWEAK)
        endif()
        set (${_packageName}_VERSION_COUNT ${_len})
    else()
        set (${_packageName}_VERSION_COUNT 0)
        set (${_packageName}_VERSION "")
    endif()
endmacro()

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
