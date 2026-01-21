cmake_minimum_required(VERSION 3.25.0)

project(Treeland
    VERSION 0.8.1
    LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 23) # Unnamed parameters in function definition is a C23 extension feature.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standard installation paths
include(GNUInstallDirs)
# Macros
include(FeatureSummary)

include(cmake/DefineTarget.cmake)

option(WITH_SUBMODULE_WAYLIB "Use the waylib from git submodule" ON)
add_feature_info(submodule_waylib WITH_SUBMODULE_WAYLIB "Use waylib from submodule")

option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)
add_feature_info(ASanSupport ADDRESS_SANITIZER "https://github.com/google/sanitizers/wiki/AddressSanitizer")

option(BUILD_TREELAND_EXAMPLES "Build clients demo to test treeland" OFF)
add_feature_info(DemoClents BUILD_TEST_CLIENTS "clients demo for testing")

option(DISABLE_DDM "Disable DDM and greeter" OFF)

if (DISABLE_DDM)
    add_compile_definitions("DISABLE_DDM")
endif()

# Alternative lock mechanism: Wayland ext-session-lock
option(EXT_SESSION_LOCK_V1 "Enable Wayland ext-session-lock support (alternative to DDM)" ON)
add_feature_info(ext_session_lock_v1 EXT_SESSION_LOCK_V1 "Enable ext-session-lock-v1 protocol support in Treeland")

if (EXT_SESSION_LOCK_V1)
    add_compile_definitions("EXT_SESSION_LOCK_V1")
endif()
# Heads-up if both are enabled: ext-session-lock is intended as an alternative to DDM
if (NOT EXT_SESSION_LOCK_V1 AND DISABLE_DDM)
    message(WARNING "no ext-session-lock support and no DDM, treeland will not have session locking ability.")
endif()

if (ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address -fno-optimize-sibling-calls -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
# NOTE: Qt keywords conflict with wlroots. but dtksystemsettings still uses it.
# add_definitions("-DQT_NO_KEYWORDS")

# NOTE: Enable Qt logging with context.
add_definitions("-DQT_MESSAGELOGCONTEXT")

set(LOCAL_QML_IMPORT_PATH "${PROJECT_BINARY_DIR}/qt/qml")

if(WITH_SUBMODULE_WAYLIB)
    set(WITH_SUBMODULE_QWLROOTS ON)
    include(${CMAKE_CURRENT_SOURCE_DIR}/qwlroots/cmake/WaylandScannerHelpers.cmake)
    add_subdirectory(waylib)
    list(APPEND LOCAL_QML_IMPORT_PATH "${PROJECT_BINARY_DIR}/waylib/src/server")
else()
    find_package(Waylib REQUIRED Server)
endif()

set(QML_IMPORT_PATH "${LOCAL_QML_IMPORT_PATH}" CACHE STRING "For LSP" FORCE)

# PKG-CONFIG
find_package(PkgConfig REQUIRED)
find_package(Qt6 CONFIG REQUIRED Core DBus Gui Qml Quick QuickControls2 LinguistTools Test QuickTest)

pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.19)

qt_standard_project_setup(REQUIRES 6.8)

# Set constants
set(TREELAND_DATA_DIR           "${CMAKE_INSTALL_DATADIR}/treeland/"               CACHE PATH      "treeland data install directory")
set(TREELAND_COMPONENTS_TRANSLATION_DIR  "${TREELAND_DATA_DIR}/translations"                  CACHE PATH      "Components translations directory")
set(TREELAND_PLUGINS_OUTPUT_PATH   "${CMAKE_BINARY_DIR}/lib/plugins"          CACHE PATH      "treeland plugins output directory")
set(TREELAND_PLUGINS_INSTALL_PATH  "${CMAKE_INSTALL_LIBDIR}/treeland/plugins" CACHE PATH      "treeland plugins install directory")

GNUInstallDirs_get_absolute_install_dir(
    TREELAND_FULL_DATA_DIR
    TREELAND_DATA_DIR
    DATADIR
)

GNUInstallDirs_get_absolute_install_dir(
    TREELAND_FULL_PLUGINS_INSTALL_PATH
    TREELAND_PLUGINS_INSTALL_PATH
    LIBDIR
)

GNUInstallDirs_get_absolute_install_dir(
    TREELAND_FULL_COMPONENTS_TRANSLATION_DIR
    TREELAND_COMPONENTS_TRANSLATION_DIR
    DATADIR
)

add_compile_definitions("TREELAND_DATA_DIR=\"${TREELAND_FULL_DATA_DIR}\"")
# NOTE:: remove force assert before stable version
add_compile_definitions("QT_FORCE_ASSERTS")

add_compile_definitions("TREELAND_PLUGINS_INSTALL_PATH=\"${TREELAND_FULL_PLUGINS_INSTALL_PATH}\"")
add_compile_definitions("TREELAND_PLUGINS_OUTPUT_PATH=\"${TREELAND_PLUGINS_OUTPUT_PATH}\"")

add_compile_definitions("TREELAND_COMPONENTS_TRANSLATION_DIR=\"${TREELAND_FULL_COMPONENTS_TRANSLATION_DIR}\"")

set(PROJECT_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/misc")

enable_testing(true)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(misc)
add_subdirectory(tests)
add_subdirectory(wallpaper-factory)

if (BUILD_TREELAND_EXAMPLES)
    add_subdirectory(examples)
endif()

# Display feature summary
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
