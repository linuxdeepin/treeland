configure_file("Constants.h.in" "Constants.h" IMMEDIATE @ONLY)

add_library(common STATIC)

target_sources(common
    PUBLIC
    FILE_SET HEADERS
    FILES
        ConfigReader.h
        Configuration.h
        Constants.h.in
        MessageHandler.h
        Messages.h
        SafeDataStream.h
        Session.h
        SignalHandler.h
        SocketWriter.h
        ThemeConfig.h
        ThemeMetadata.h
        VirtualTerminal.h
        XAuth.h
        MFAInfoList.h
        QVariantMap.h
        ArrayInt.h
        DBusTypes.h
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/Constants.h
        ConfigReader.cpp
        Configuration.cpp
        SafeDataStream.cpp
        Session.cpp
        SignalHandler.cpp
        SocketWriter.cpp
        ThemeConfig.cpp
        ThemeMetadata.cpp
        VirtualTerminal.cpp
        XAuth.cpp
        LogindDBusTypes.cpp
        MFAInfoList.cpp
        QVariantMap.cpp
        ArrayInt.cpp
)

target_include_directories(common
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_BINARY_DIR}/src/common
)

target_link_libraries(common
    PUBLIC
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::Qml
        ${LIBXAU_LINK_LIBRARIES}
)
