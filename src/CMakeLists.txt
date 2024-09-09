if (QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)
find_package(DDM REQUIRED COMPONENTS Common)
find_package(Qt6 CONFIG REQUIRED ShaderTools)
# qt_finalize_target will collect all executable's private dependencies that are CMake targets

add_subdirectory(utils)
add_subdirectory(qml)
add_subdirectory(modules)
add_subdirectory(greeter)
add_subdirectory(treeland-wallpaper)
add_subdirectory(treeland-shortcut)

qt_add_dbus_adaptor(DBUS_ADAPTOR "${CMAKE_SOURCE_DIR}/misc/dbus/org.deepin.compositor1.xml" "treeland.h" TreeLand::TreeLand)

set(BIN_NAME treeland)

qt_add_executable(${BIN_NAME})

target_sources(${BIN_NAME} PUBLIC
FILE_SET HEADERS
FILES
    treeland.h
)

set(RESOURCES
)

qt_add_resources(${BIN_NAME} "treeland_assets"
    PREFIX "/dsg"
    BASE ${PROJECT_RESOURCES_DIR}
    FILES
        ${PROJECT_RESOURCES_DIR}/icons/close.dci
        ${PROJECT_RESOURCES_DIR}/icons/multitaskview_close.dci
)

target_sources(${BIN_NAME} PRIVATE
    treeland.cpp
    ${DBUS_ADAPTOR}
)

target_link_libraries(${BIN_NAME}
    PRIVATE
        DDM::Common
        greeterplugin
        Qt6::DBus
        Waylib::WaylibServer
        Dtk6::Declarative
        utilsplugin
        treeland-qmlplugin
        foreign-toplevel
        primary-output
        personalization
        shortcut
        wallpaper-color
        window-management
        virtual-output
)

target_compile_definitions(${BIN_NAME}
    PRIVATE
    WLR_USE_UNSTABLE
)

qt_add_shaders(${BIN_NAME} "_shaders_ng"
    BATCHABLE
    PRECOMPILE
    PREFIX
        "/shaders"
    BASE
        ${PROJECT_RESOURCES_DIR}/shaders
    FILES
        ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.vert
        ${PROJECT_RESOURCES_DIR}/shaders/radiussmoothtexture.frag
)

install(TARGETS ${BIN_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

## systemd socket

pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

qt_add_executable(treeland-sd
    systemd-socket.cpp
)

target_link_libraries(treeland-sd
    PRIVATE
        Qt6::DBus
        PkgConfig::Systemd
)

install(TARGETS treeland-sd RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
