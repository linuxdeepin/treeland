pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_qml_module(treeland-quick-utils
    URI TreeLand.Utils
    VERSION "1.0"
    STATIC
    SOURCES
        helper.cpp
        filterproxymodel.cpp
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Utils

)

target_sources(treeland-quick-utils PUBLIC
FILE_SET HEADERS
FILES
    helper.h
    filterproxymodel.h
)

target_compile_definitions(treeland-quick-utils
    PRIVATE
    WLR_USE_UNSTABLE
)

target_link_libraries(treeland-quick-utils
    PUBLIC
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
        PkgConfig::WLROOTS
)
