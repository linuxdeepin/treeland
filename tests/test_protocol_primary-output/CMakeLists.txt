# A user protocol requires a real Session. Keep the legacy build target as an
# alias to the production-fixture protocol test instead of constructing an
# unowned manager on a bare WServer.
if(TARGET test_treeland_output_manager_v2)
    add_custom_target(test_protocol_primary-output
        DEPENDS test_treeland_output_manager_v2)
endif()
