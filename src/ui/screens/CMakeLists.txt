qt_add_library(moligram_screens STATIC)

qt_add_qml_module(moligram_screens
    URI Moligram.Screens
    DEPENDENCIES Moligram.Design Moligram.Controls
    QML_FILES
        ChatListPane.qml
        ChatPane.qml
        MainWindow.qml
        PaneSplitter.qml
        Rail.qml)
