find_package(Qt6 REQUIRED COMPONENTS WaylandClient Quick Core Gui)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)
find_package(PkgConfig)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/ CACHE STRING "" FORCE)

qt_add_executable(treeland-wallpaper
    main.cpp
)

qt_generate_wayland_protocol_client_sources(treeland-wallpaper
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/treeland-personalization-manager-v1.xml
)

set(QML_FILES
    main.qml
    Wallpaper.qml
    WallpaperCard.qml
    WallpaperSetting.qml
    PropertyItemDelegate.qml
)

qt_add_qml_module(treeland-wallpaper
    URI Wallpaper
    VERSION "1.0"
    SOURCES
        personalizationmangerclient.cpp
        personalizationmangerclient.h
        wallpapercardmodel.cpp
        wallpapercardmodel.h
    QML_FILES ${QML_FILES}
)

qt_add_lupdate(TS_FILES
        translations/wallpaper.zh_CN.ts
        translations/wallpaper.en_US.ts
    SOURCE_TARGETS treeland-wallpaper
    LUPDATE_TARGET treeland-wallpaper_lupdate
    NO_GLOBAL_TARGET
)

add_dependencies(treeland-wallpaper treeland-wallpaper_lupdate)
set(TRANSLATED_FILES)
qt_add_lrelease(TS_FILES
        translations/wallpaper.zh_CN.ts
        translations/wallpaper.en_US.ts
    LRELEASE_TARGET treeland-wallpaper_lrelease
    QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
)
add_dependencies(treeland-wallpaper_lrelease treeland-wallpaper_lrelease)

install(FILES ${TRANSLATED_FILES} DESTINATION ${COMPONENTS_TRANSLATION_DIR})

target_link_libraries(treeland-wallpaper
    PRIVATE
        common
        Qt${QT_MAJOR_VERSION}::Gui
        Qt::WaylandClient
        Dtk6::Declarative
)

install(TARGETS treeland-wallpaper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
