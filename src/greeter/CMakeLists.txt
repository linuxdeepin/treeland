set(GREETER_SOURCES
    GreeterApp.h
    GreeterProxy.h
    SessionModel.h
    UserModel.h
    User.h
    LogoProvider.h
    GreeterApp.cpp
    GreeterProxy.cpp
    SessionModel.cpp
    UserModel.cpp
    User.cpp
    LogoProvider.cpp
    greeter_assets.qrc
)

find_package(Dtk6 REQUIRED COMPONENTS SystemSettings Declarative)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ CACHE STRING "" FORCE)

set_source_files_properties(GreeterModel.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set(QML_FILES
    Greeter.qml
    UserList.qml
    SessionList.qml
    UserInput.qml
    GreeterModel.qml
    Center.qml
    LoginAnimation.qml
    RoundBlur.qml
    PowerList.qml
    TimeDateWidget.qml
    HintLabel.qml
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
    QML_FILES ${QML_FILES}
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Greeter
)

qt_add_lupdate(greeterplugin
    TS_FILES
        translations/greeter.zh_CN.ts
        translations/greeter.en_US.ts
    NO_GLOBAL_TARGET
)

add_dependencies(greeterplugin greeterplugin_lupdate)

set(TRANSLATED_FILES)
qt_add_lrelease(greeterplugin
    TS_FILES
        translations/greeter.zh_CN.ts
        translations/greeter.en_US.ts
    QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
)
add_dependencies(greeterplugin_lrelease greeterplugin_lupdate)

install(FILES ${TRANSLATED_FILES} DESTINATION ${COMPONENTS_TRANSLATION_DIR})

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
        Dtk6::SystemSettings
)

if(JOURNALD_FOUND)
    target_link_libraries(greeterplugin PUBLIC ${JOURNALD_LIBRARIES})
endif()

# Translations
add_dependencies(greeterplugin components-translation)
