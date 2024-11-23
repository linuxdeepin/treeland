set(GREETER_SOURCES
    greeterapp.h
    greeterapp.cpp
    greeterproxy.h
    greeterproxy.cpp
    sessionmodel.h
    sessionmodel.cpp
    usermodel.h
    usermodel.cpp
    user.h
    user.cpp
    logoprovider.h
    logoprovider.cpp
)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Dtk6 REQUIRED COMPONENTS SystemSettings Declarative)
find_package(DDM REQUIRED COMPONENTS Auth)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PAM REQUIRED IMPORTED_TARGET pam)

set_source_files_properties("${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.xml" PROPERTIES
    NO_NAMESPACE ON
    CLASSNAME DisplayManager
)
set_source_files_properties("${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.Seat.xml" PROPERTIES
    NO_NAMESPACE ON
    CLASSNAME DisplaySeat
)

set_source_files_properties("${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.Session.xml" PROPERTIES
    NO_NAMESPACE ON
    CLASSNAME DisplaySession
)

qt_add_dbus_interface(DDM_DBUS_SOURCES "${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.xml"          DisplayManager)
qt_add_dbus_interface(DDM_DBUS_SOURCES "${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.Seat.xml"     DisplayManagerSeat)
qt_add_dbus_interface(DDM_DBUS_SOURCES "${CMAKE_SOURCE_DIR}/misc/interfaces/org.freedesktop.DisplayManager.Session.xml"  DisplayManagerSession)

set_source_files_properties(qml/GreeterModel.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set(QML_FILES
    qml/Greeter.qml
    qml/ShutdownView.qml
    qml/LockView.qml
    qml/UserList.qml
    qml/SessionList.qml
    qml/UserInput.qml
    qml/GreeterModel.qml
    qml/LoginAnimation.qml
    qml/RoundBlur.qml
    qml/PowerList.qml
    qml/TimeDateWidget.qml
    qml/HintLabel.qml
    qml/QuickAction.qml
    qml/ControlAction.qml
    qml/ShutdownButton.qml
)

set(LIB_NAME greeter)

qt_add_qml_module(${LIB_NAME}
    URI Treeland.Greeter
    SHARED
    VERSION 1.0
    CLASS_NAME
        GreeterExtensionPlugin
    SOURCES
        ${GREETER_SOURCES}
        ${DDM_DBUS_SOURCES}
    RESOURCE_PREFIX
        /qt/qml
    QML_FILES ${QML_FILES}
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/Treeland/Greeter
)

qt_add_resources(${LIB_NAME} "greeter_assets"
    PREFIX "/dsg"
    BASE ${PROJECT_RESOURCES_DIR}
    FILES
        ${PROJECT_RESOURCES_DIR}/icons/login_capslock.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_display_password.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_hidden_password.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_keyboard.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_lock.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_open.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_power.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_reboot.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_sleep.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_user.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_hint.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_logout.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_poweroff.dci
        ${PROJECT_RESOURCES_DIR}/icons/login_suspend.dci
        ${PROJECT_RESOURCES_DIR}/icons/backToLightdm.dci
        ${PROJECT_RESOURCES_DIR}/icons/logo.svg
)

set(TRANSLATED_FILES)
if (${QT_VERSION_MINOR} LESS 7)
    qt_add_lupdate(greeterplugin
        TS_FILES
            translations/greeter.zh_CN.ts
            translations/greeter.en_US.ts
        NO_GLOBAL_TARGET
    )

    qt_add_lrelease(${LIB_NAME}
        TS_FILES
            translations/greeter.zh_CN.ts
            translations/greeter.en_US.ts
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
elseif (${QT_VERSION_MINOR} GREATER_EQUAL 7)
    qt_add_lupdate(
        SOURCE_TARGETS ${LIB_NAME}
        TS_FILES
            translations/greeter.zh_CN.ts
            translations/greeter.en_US.ts
        NO_GLOBAL_TARGET
    )

    qt_add_lrelease(
        TS_FILES
            translations/greeter.zh_CN.ts
            translations/greeter.en_US.ts
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
endif()

target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/greeter/
)

target_link_libraries(${LIB_NAME}
    PRIVATE
        DDM::Auth
        PkgConfig::PAM
    PUBLIC
        Qt6::Gui
        Qt6::Qml
        Dtk6::Declarative
        Dtk6::SystemSettings
)

install(FILES ${TRANSLATED_FILES} DESTINATION ${TREELAND_COMPONENTS_TRANSLATION_DIR})
install(TARGETS ${LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
