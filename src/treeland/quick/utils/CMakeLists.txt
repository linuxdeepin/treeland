pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)

qt_add_qml_module(treeland-quick-utils
    URI TreeLand.Utils
    VERSION "1.0"
    STATIC
    SOURCES
        helper.cpp
        treelandhelper.cpp
        dockpreviewfilter.cpp
    RESOURCE_PREFIX
        /qt/qml
)

target_sources(treeland-quick-utils PUBLIC
FILE_SET HEADERS
FILES
    helper.h
    treelandhelper.h
    dockpreviewfilter.h
)

target_compile_definitions(treeland-quick-utils
    PRIVATE
    WLR_USE_UNSTABLE
)

target_link_libraries(treeland-quick-utils
    PUBLIC
        $<IF:$<BOOL:${WITH_SUBMODULE_WAYLIB}>, waylibserver, Waylib::WaylibServer>
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
        PkgConfig::WLROOTS
)
