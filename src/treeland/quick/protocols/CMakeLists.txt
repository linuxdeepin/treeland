pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_qml_module(treeland-quick-protocols
    URI TreeLand.Protocols
    VERSION "1.0"
    STATIC
    SOURCES
        extforeigntoplevellist.cpp
        outputmanagement.cpp
        foreigntoplevelmanagerv1.cpp
        shortcutmanager.cpp
        socketmanager.cpp
        waylandsocketproxy.cpp
        qwpersonalizationmanager.cpp
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Protocols
)

target_sources(treeland-quick-protocols PUBLIC
FILE_SET HEADERS
FILES
    extforeigntoplevellist.h
    outputmanagement.h
    foreigntoplevelmanagerv1.h
    shortcutmanager.h
    socketmanager.h
    waylandsocketproxy.h
    qwpersonalizationmanager.h
)

target_compile_definitions(treeland-quick-protocols
    PRIVATE
    WLR_USE_UNSTABLE
)

target_link_libraries(treeland-quick-protocols
    PUBLIC
        treeland-protocols
        treeland-quick-utils
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Quick
)
