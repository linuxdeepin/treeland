set(SRCS
    ${CMAKE_SOURCE_DIR}/src/auth/Auth.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/Auth.h
    ${CMAKE_SOURCE_DIR}/src/auth/AuthMessages.h
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthPrompt.h
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/AuthRequest.h
)

add_library(auth STATIC ${SRCS})

target_include_directories(auth
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/auth
)

target_link_libraries(auth
    PUBLIC
        common
        Qt${QT_MAJOR_VERSION}::Network
        Qt${QT_MAJOR_VERSION}::Qml
)

if(PAM_FOUND)
    target_link_libraries(auth PUBLIC ${PAM_LIBRARIES})
else()
    target_link_libraries(auth PUBLIC crypt)
endif()
