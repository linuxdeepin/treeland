set_source_files_properties(GreeterModel.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

find_package(Qt6 COMPONENTS Quick QuickControls2 REQUIRED)
find_package(Dtk6 REQUIRED COMPONENTS Declarative)

set(QML_FILES
    Greeter.qml
    UserList.qml
    SessionList.qml
    UserInput.qml
    GreeterModel.qml
    LoginAnimation.qml
    RoundBlur.qml
    PowerList.qml
    TimeDateWidget.qml
    HintLabel.qml
    QuickAction.qml
    ControlAction.qml
)

qt_add_qml_module(${LIB_NAME}-qml
    URI TreeLand.Greeter
    STATIC
    VERSION 1.0
    RESOURCE_PREFIX
        /qt/qml
    QML_FILES ${QML_FILES}
    OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/qt/qml/TreeLand/Greeter
)

target_link_libraries(${LIB_NAME}-qml
    PUBLIC
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        Dtk6::Declarative
        Waylib::WaylibServer
)

