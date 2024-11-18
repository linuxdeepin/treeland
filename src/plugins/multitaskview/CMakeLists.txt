find_package(Qt6 COMPONENTS Core Quick Concurrent REQUIRED)

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
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Plugins/
)

target_link_libraries(multitaskview PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Concurrent
    treeland_core
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

install(TARGETS multitaskview DESTINATION ${TREELAND_PLUGINS_INSTALL_PATH})