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
        wallpaperprovider.cpp
        gestures.cpp
        togglablegesture.cpp
        logstream.cpp
        tquickradiuseffect.cpp
        tsgradiusimagenode.cpp
        global.cpp
        wwallpaperimage.cpp
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
    wallpaperprovider.h
    gestures.h
    togglablegesture.h
    logstream.h
    tquickradiuseffect.h
    tquickradiuseffect_p.h
    tsgradiusimagenode.h
    global.h
    wwallpaperimage.h
)

target_compile_definitions(utils
    PRIVATE
    WLR_USE_UNSTABLE
)

target_include_directories(utils
    PRIVATE
        ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/src/modules
)

target_link_libraries(utils
    PUBLIC
        PkgConfig::WLROOTS
        PkgConfig::LIBINPUT
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        capture
        Qt6::QuickPrivate
)
