add_subdirectory(qml)
add_subdirectory(protocols)
add_subdirectory(utils)

qt_add_library(treeland-quick STATIC)

target_link_libraries(treeland-quick
    PUBLIC
        treeland-quick-protocolsplugin
        treeland-quick-utilsplugin
        treeland-qmlplugin
)
