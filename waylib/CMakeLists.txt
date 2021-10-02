cmake_minimum_required(VERSION 3.1.0)

project(Waylib
    VERSION 0.0.1
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Use Qt5
set(QT_VERSION_MAJOR 5)

# For Unix/Linux
include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(examples)
