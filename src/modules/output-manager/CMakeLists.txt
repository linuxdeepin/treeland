find_package(TreelandProtocols REQUIRED)

ws_generate_local(server ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-output-manager-v1.xml treeland-output-management-protocol)

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "treeland-protocol-output-management-v1"
)

impl_treeland(
    NAME
        module_primaryoutput
    SOURCE
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/impl/output_manager_impl.h
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/outputmanagement.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/output-manager/impl/output_manager_impl.cpp
        ${WAYLAND_PROTOCOLS_OUTPUTDIR}/treeland-output-management-protocol.c
)
