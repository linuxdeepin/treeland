set(GREETER_SOURCES
    GreeterApp.h
    GreeterProxy.h
    SessionModel.h
    UserModel.h
    GreeterApp.cpp
    GreeterProxy.cpp
    SessionModel.cpp
    UserModel.cpp
)

find_package(Dtk6 REQUIRED COMPONENTS Declarative)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ CACHE STRING "" FORCE)

set_source_files_properties(GreeterModel.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(greeterplugin
    URI TreeLand.Greeter
    STATIC
    VERSION 1.0
    PLUGIN_TARGET greeterplugin
    NO_PLUGIN_OPTIONAL
    NO_GENERATE_PLUGIN_SOURCE
    CLASS_NAME
        GreeterExtensionPlugin
    SOURCES
        ${GREETER_SOURCES}
    RESOURCE_PREFIX
        /qt/qml
    QML_FILES Greeter.qml
    QML_FILES UserList.qml
    QML_FILES SessionList.qml
    QML_FILES UserInput.qml
    QML_FILES GreeterModel.qml
    QML_FILES Center.qml
    QML_FILES LoginAnimation.qml
    QML_FILES RoundBlur.qml
    QML_FILES PowerList.qml
    QML_FILES TimeDateWidget.qml
)

target_include_directories(greeterplugin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/greeter/
)

target_link_libraries(greeterplugin
    PUBLIC
        common
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Qml
        Dtk6::Declarative
)

if(JOURNALD_FOUND)
    target_link_libraries(greeterplugin PUBLIC ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(greeterplugin components-translation)
