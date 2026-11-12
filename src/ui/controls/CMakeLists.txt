qt_add_library(moligram_controls STATIC)

set_source_files_properties(Icons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(moligram_controls
    URI Moligram.Controls
    DEPENDENCIES Moligram.Design
    QML_FILES
        EmptyState.qml
        Grain.qml
        Icon.qml
        IconButton.qml
        Icons.qml
        Label.qml
        SearchField.qml)
