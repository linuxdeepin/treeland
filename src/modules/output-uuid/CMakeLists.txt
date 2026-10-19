find_package(TreelandProtocols 0.7.0 REQUIRED)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-unstable-v1.xml
    BASENAME treeland-output-uuid-unstable-v1
)

# treeland-output-uuid-head-unstable-v1 takes a zwlr_output_head_v1 argument, so
# its generated code needs zwlr_output_head_v1_interface. waylib-wlroots only
# exports wlr_*/waylib_* (see 3rdparty/wlroots/wlroots.syms), which hides that
# symbol, so generate the wlr-output-management protocol into libtreeland too.
waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${CMAKE_SOURCE_DIR}/3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml
    BASENAME wlr-output-management-unstable-v1
)

waylib_generate_qtwayland_server_protocol(libtreeland
    PROTOCOL ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-uuid-head-unstable-v1.xml
    BASENAME treeland-output-uuid-head-unstable-v1
)

impl_treeland(
    NAME
        module_output_uuid
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-uuid/outputuuidmanagerinterfacev1.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-uuid/outputuuidmanagerinterfacev1.cpp
    INCLUDE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
