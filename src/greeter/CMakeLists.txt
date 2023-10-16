set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.deepin.dde.Authenticate1.xml" PROPERTIES
    INCLUDE "DBusTypes.h"
    CLASSNAME Authenticate1
    NO_NAMESPACE True
)

set_source_files_properties("${CMAKE_SOURCE_DIR}/data/interfaces/org.deepin.dde.Authenticate1.Session2.xml" PROPERTIES
    INCLUDE "DBusTypes.h"
    CLASSNAME AuthenticateSession2
    NO_NAMESPACE True
)

qt_add_dbus_interface(AUTHENTICATE_SOURCE "${CMAKE_SOURCE_DIR}/data/interfaces/org.deepin.dde.Authenticate1.xml" "Authenticate1")

qt_add_dbus_interface(AUTHENTICATE_SOURCE "${CMAKE_SOURCE_DIR}/data/interfaces/org.deepin.dde.Authenticate1.Session2.xml" "AuthenticateSession2")

set(GREETER_SOURCES
    ${AUTHENTICATE_SOURCE}
    GreeterApp.h
    GreeterProxy.h
    SessionModel.h
    UserModel.h
    GreeterApp.cpp
    GreeterProxy.cpp
    KeyboardLayout.cpp
    KeyboardModel.cpp
    ScreenModel.cpp
    SessionModel.cpp
    UserModel.cpp
    waylandkeyboardbackend.cpp
    waylandkeyboardbackend.h
    XcbKeyboardBackend.cpp
    DeepinAuth/deepinauthframework.h
    DeepinAuth/deepinauthframework.cpp
    DeepinAuth/authcommon.h
    DeepinAuth/greeterworker.h
    DeepinAuth/greeterworker.cpp
    DeepinAuth/sessionbasemodel.h
    DeepinAuth/sessionbasemodel.cpp
    userinfo.h
    userinfo.cpp
)

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
    QML_FILES
      Greeter.qml
      UserList.qml
      SessionList.qml
      UserInput.qml
      GreeterModel.qml
)

target_include_directories(greeterplugin
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/greeter/
        ${CMAKE_SOURCE_DIR}/src/greeter/DeepinAuth
)

target_link_libraries(greeterplugin
    PUBLIC
        common
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Qml
        Qt${QT_MAJOR_VERSION}::Quick
        ${LIBXCB_LIBRARIES}
        ${LIBXKB_LIBRARIES}
)

if(JOURNALD_FOUND)
    target_link_libraries(greeterplugin PUBLIC ${JOURNALD_LIBRARIES})
endif()

if(PAM_FOUND)
    target_link_libraries(greeterplugin PUBLIC ${PAM_LIBRARIES})
else()
    target_link_libraries(greeterplugin PUBLIC crypt)
endif()

# Translations
add_dependencies(greeterplugin components-translation)
