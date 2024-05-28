if (QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} ${PROJECT_BINARY_DIR}/qt/qml CACHE STRING "" FORCE)

add_subdirectory(greeter)
add_subdirectory(treeland)
