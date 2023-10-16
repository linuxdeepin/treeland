add_library(auth STATIC)

target_sources(auth
    PUBLIC
    FILE_SET HEADERS
    FILES
        Auth.h
        AuthMessages.h
        AuthPrompt.h
        AuthRequest.h
    PRIVATE
        Auth.cpp
        AuthPrompt.cpp
        AuthRequest.cpp
)

target_link_libraries(auth
    PUBLIC
        common
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::Qml
        Qt${QT_MAJOR_VERSION}::DBus
)

if(PAM_FOUND)
    target_link_libraries(auth PUBLIC ${PAM_LIBRARIES})
else()
    target_link_libraries(auth PUBLIC crypt)
endif()
