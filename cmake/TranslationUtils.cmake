# TranslationUtils.cmake
# Provides common functions for handling translation files

function(setup_translations TARGET_NAME TRANSLATION_PREFIX)
    # Automatically discover all translation files
    file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/${TRANSLATION_PREFIX}.*.ts")
    
    # Filter out non-translation files (if any)
    list(FILTER TS_FILES INCLUDE REGEX ".*\\.ts$")
    
    # Set translation files variable
    set(TRANSLATED_FILES)
    
    # Add lupdate target
    qt_add_lupdate(
        SOURCE_TARGETS ${TARGET_NAME}
        TS_FILES ${TS_FILES}
        NO_GLOBAL_TARGET
    )
    
    # Add lrelease target
    qt_add_lrelease(
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
    
    # Install translation files
    install(FILES ${TRANSLATED_FILES} DESTINATION ${TREELAND_COMPONENTS_TRANSLATION_DIR})
    
    # Set TRANSLATED_FILES variable to parent scope
    set(TRANSLATED_FILES ${TRANSLATED_FILES} PARENT_SCOPE)
endfunction()

function(setup_main_translations TARGET_NAME)
    # Automatically discover all main translation files
    file(GLOB TS_FILES "${CMAKE_SOURCE_DIR}/translations/treeland.*.ts")
    
    # Filter out non-translation files (if any)
    list(FILTER TS_FILES INCLUDE REGEX ".*\\.ts$")
    
    # Set translation files variable
    set(TRANSLATED_FILES)
    
    # Add lupdate target
    qt_add_lupdate(
        SOURCE_TARGETS ${TARGET_NAME}
        TS_FILES ${TS_FILES}
        NO_GLOBAL_TARGET
    )
    
    # Add lrelease target
    qt_add_lrelease(
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE TRANSLATED_FILES
    )
    
    # Install translation files
    install(FILES ${TRANSLATED_FILES} DESTINATION ${TREELAND_COMPONENTS_TRANSLATION_DIR})
    
    # Set TRANSLATED_FILES variable to parent scope
    set(TRANSLATED_FILES ${TRANSLATED_FILES} PARENT_SCOPE)
endfunction()
