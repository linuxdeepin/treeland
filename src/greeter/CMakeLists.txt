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

install(TARGETS ${LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
