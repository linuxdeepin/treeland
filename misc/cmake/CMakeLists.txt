configure_file(TreeLandProtocolsConfig.cmake.in TreeLandProtocolsConfig.cmake)
configure_file(treeland-protocols.pc.in treeland-protocols.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TreeLandProtocolsConfig.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/TreeLandProtocols/)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/treeland-protocols.pc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pkgconfig)
