find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-virtual-output-manager-v1.xml treeland-virtual-output-manager-protocol)

impl_treeland(
    NAME
        module_virtualoutput
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/virtualoutputmanager.h
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/impl/virtual_output_manager_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/virtualoutputmanager.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/virtual-output/impl/virtual_output_manager_impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-virtual-output-manager-protocol.c
    LINK
        PkgConfig::WLROOTS
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)
