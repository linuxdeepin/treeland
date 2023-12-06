set_source_files_properties(QmlHelper.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(treeland-qml
    URI TreeLand
    VERSION "1.0"
    STATIC
    QML_FILES
        Main.qml
        CloseAnimation.qml
        OutputDelegate.qml
        QmlHelper.qml
        StackToplevelHelper.qml
        StackWorkspace.qml
        TiledToplevelHelper.qml
        TiledWorkspace.qml
        WindowDecoration.qml
        XdgSurface.qml
        InputPopupSurface.qml
        LayerSurface.qml
        MiniDock.qml
        WindowsSwitcher.qml
        WindowsSwitcherPreview.qml
    RESOURCE_PREFIX
        /qt/qml
)

