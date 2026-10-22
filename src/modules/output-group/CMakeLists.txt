find_package(TreelandProtocols REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-group-manager-unstable-v1.xml
    BASENAME treeland-output-group-manager-unstable-v1
)

impl_treeland(
    NAME
        module_outputgroup
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-group/outputgroupmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-group/outputgroupmanagerinterfacev1.cpp
    LINK
        Wlroots::wlroots
        Waylib::WaylibServer
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
)
