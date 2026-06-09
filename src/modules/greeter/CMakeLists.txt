add_library(treelandremote_qro OBJECT)
set_target_properties(treelandremote_qro PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(treelandremote_qro PRIVATE Qt6::RemoteObjects)
qt_add_repc_sources(treelandremote_qro
    ${CMAKE_CURRENT_SOURCE_DIR}/treelandremote.rep
)

target_link_libraries(libtreeland PRIVATE treelandremote_qro)
target_include_directories(libtreeland PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ddmremoteobjectv1.cpp PROPERTIES SKIP_AUTOMOC ON)

setup_translations(libtreeland greeter)
