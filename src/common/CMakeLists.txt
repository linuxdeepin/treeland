configure_file("Constants.h.in" "Constants.h" IMMEDIATE @ONLY)

set(SRCS
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ConfigReader.h
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Configuration.h
    ${CMAKE_SOURCE_DIR}/src/common/Constants.h.in
    ${CMAKE_SOURCE_DIR}/src/common/MessageHandler.h
    ${CMAKE_SOURCE_DIR}/src/common/Messages.h
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SafeDataStream.h
    ${CMAKE_SOURCE_DIR}/src/common/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Session.h
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SignalHandler.h
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/common/SocketWriter.h
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeConfig.h
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.cpp
    ${CMAKE_SOURCE_DIR}/src/common/ThemeMetadata.h
    ${CMAKE_SOURCE_DIR}/src/common/VirtualTerminal.cpp
    ${CMAKE_SOURCE_DIR}/src/common/VirtualTerminal.h
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.cpp
    ${CMAKE_SOURCE_DIR}/src/common/XAuth.h
    ${CMAKE_SOURCE_DIR}/src/common/LogindDBusTypes.cpp

    ${CMAKE_BINARY_DIR}/src/common/Constants.h
)

add_library(common STATIC ${SRCS})

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
