cmake_minimum_required(VERSION 3.1.0)

project(Waylib
    VERSION 0.0.1
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(USE_QT5 "Use the Qt5 library" ON)

if (${USE_QT5} STREQUAL "ON")
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# For Unix/Linux
include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(examples)
