find_package(Qt6 REQUIRED COMPONENTS Core Network RemoteObjects)
add_executable(treeland-ddm-debug
    main.cpp
    cli.cpp
    dummyddmstate.cpp
    dummyddmservice.cpp
    dummytreelandclient.cpp
)

target_link_libraries(treeland-ddm-debug
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::RemoteObjects
)

target_include_directories(treeland-ddm-debug
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/modules/greeter
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/treeland-ddm-debug_autogen/include
)

qt_add_repc_sources(treeland-ddm-debug
    ${CMAKE_SOURCE_DIR}/src/modules/greeter/ddmremote.rep
    ${CMAKE_CURRENT_SOURCE_DIR}/dummyddmcontrol.rep
)
qt_add_repc_replicas(treeland-ddm-debug
    ${CMAKE_SOURCE_DIR}/src/modules/greeter/treelandremote.rep
    ${CMAKE_CURRENT_SOURCE_DIR}/dummyddmcontrol.rep
)
