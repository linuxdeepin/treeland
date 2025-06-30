find_package(QT NAMES Qt6 COMPONENTS Core Quick Concurrent REQUIRED)
find_package(Dtk6 REQUIRED COMPONENTS SystemSettings Declarative)

# Include translation utilities
include(${CMAKE_SOURCE_DIR}/cmake/TranslationUtils.cmake)

set_source_files_properties(qml/GreeterModel.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_library(lockscreen SHARED
    lockscreenplugin.h
    lockscreenplugin.cpp
)

qt_add_qml_module(lockscreen
    URI LockScreen
    SOURCES
        logoprovider.h
        logoprovider.cpp
    QML_FILES
        qml/GreeterModel.qml
        qml/HintLabel.qml
        qml/LoginAnimation.qml
        qml/QuickAction.qml
        qml/SessionList.qml
        qml/ShutdownView.qml
        qml/UserInput.qml
        qml/ControlAction.qml
        qml/Greeter.qml
        qml/LockView.qml
        qml/PowerList.qml
        qml/RoundBlur.qml
        qml/ShutdownButton.qml
        qml/TimeDateWidget.qml
        qml/UserList.qml
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Plugins/LockScreen
)

target_include_directories(lockscreen PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/treeland/plugins/lockscreen>
)

target_link_libraries(lockscreen PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Concurrent
    libtreeland
)

set_target_properties(lockscreen PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${TREELAND_PLUGINS_OUTPUT_PATH}"
)

# Use translation utilities to setup translations
setup_translations(lockscreen lockscreen)

install(TARGETS lockscreen DESTINATION ${TREELAND_PLUGINS_INSTALL_PATH})
