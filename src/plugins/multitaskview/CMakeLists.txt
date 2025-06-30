find_package(QT NAMES Qt6 COMPONENTS Core Quick Concurrent REQUIRED)

# Include translation utilities
include(${CMAKE_SOURCE_DIR}/cmake/TranslationUtils.cmake)

qt_add_library(multitaskview SHARED
    multitaskviewplugin.h
    multitaskviewplugin.cpp
)

qt_add_qml_module(multitaskview
    URI MultitaskView
    SOURCES
        multitaskview.h
        multitaskview.cpp
    QML_FILES
        qml/MultitaskviewProxy.qml
        qml/WindowSelectionGrid.qml
        qml/WorkspaceSelectionList.qml
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Plugins/MultitaskView
)

target_link_libraries(multitaskview PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Concurrent
    libtreeland
)

target_include_directories(multitaskview PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/treeland/plugins/lockscreen>
)

set_target_properties(multitaskview PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${TREELAND_PLUGINS_OUTPUT_PATH}"
)

qt_add_resources(multitaskview "multitaskview_assets"
    PREFIX "/dsg/icons"
    BASE ${PROJECT_RESOURCES_DIR}/icons
    FILES
        ${PROJECT_RESOURCES_DIR}/icons/multitaskview_close.dci
)

# Use translation utilities to setup translations
setup_translations(multitaskview multitaskview)

install(TARGETS multitaskview DESTINATION ${TREELAND_PLUGINS_INSTALL_PATH})
