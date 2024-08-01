pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots)
pkg_search_module(LIBINPUT REQUIRED IMPORTED_TARGET libinput)

qt_add_qml_module(utils
    URI TreeLand.Utils
    VERSION "1.0"
    STATIC
    SOURCES
        helper.cpp
        filterproxymodel.cpp
        surfaceitemfactory.cpp
        inputdevice.cpp
        wallpaperproxy.cpp
        wallpapermanager.cpp
        wallpapercontroller.cpp
        gestures.cpp
        togglablegesture.cpp
    RESOURCE_PREFIX
        /qt/qml
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Utils
)

target_sources(utils PUBLIC
FILE_SET HEADERS
FILES
    helper.h
    filterproxymodel.h
    inputdevice.h
    wallpaperproxy.h
    wallpapermanager.h
    wallpapercontroller.h
    gestures.h
    togglablegesture.h
)

target_compile_definitions(utils
    PRIVATE
    WLR_USE_UNSTABLE
)

target_include_directories(utils
    PRIVATE
        ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
)

target_link_libraries(utils
    PUBLIC
        PkgConfig::WLROOTS
        PkgConfig::LIBINPUT
        Waylib::WaylibServer
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
)
