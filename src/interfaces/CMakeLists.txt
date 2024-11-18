find_package(Qt6 COMPONENTS Core REQUIRED)

add_library(treeland_interface INTERFACE)

target_include_directories(treeland_interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/treeland>
)

set(INTERFACE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/plugininterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/proxyinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/multitaskviewinterface.h
)

target_sources(treeland_interface
INTERFACE
    ${INTERFACE_HEADERS}
)

target_link_libraries(treeland_interface INTERFACE Qt6::Core)

install(
    FILES ${INTERFACE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/treeland
)
