cmake_minimum_required(VERSION 3.25.0)

project(TreeLand
    VERSION 0.2.2
    LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standard installation paths
include(GNUInstallDirs)
# Macros
include(FeatureSummary)

option(WITH_SUBMODULE_WAYLIB "Use the waylib from git submodule" OFF)
add_feature_info(submodule_waylib WITH_SUBMODULE_WAYLIB "Use waylib from submodule")

if(WITH_SUBMODULE_WAYLIB)
    set(WITH_SUBMODULE_QWLROOTS ON)
    add_subdirectory(waylib)
else()
    find_package(Waylib REQUIRED Server)
endif()

# PKG-CONFIG
find_package(PkgConfig)

find_package(Qt6 CONFIG REQUIRED Core DBus Gui Qml Quick QuickControls2 LinguistTools Test QuickTest)
qt_standard_project_setup(REQUIRES 6.6)

# Set constants
set(TREELAND_DATA_DIR           "${CMAKE_INSTALL_FULL_DATADIR}/treeland/"               CACHE PATH      "treeland data install directory")
set(COMPONENTS_TRANSLATION_DIR  "${TREELAND_DATA_DIR}/translations"                  CACHE PATH      "Components translations directory")

add_compile_definitions("TREELAND_DATA_DIR=\"${TREELAND_DATA_DIR}\"")

add_subdirectory(src)
