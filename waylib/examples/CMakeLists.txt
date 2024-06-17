include_directories(PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/server
    ${PROJECT_SOURCE_DIR}/src/server/kernel
    ${PROJECT_SOURCE_DIR}/src/server/qtquick
)

add_subdirectory(tinywl)
add_subdirectory(blur)
add_subdirectory(surface-delegate)
add_subdirectory(outputviewport)
