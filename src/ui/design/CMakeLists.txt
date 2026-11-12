qt_add_library(moligram_design STATIC)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(moligram_design
    URI Moligram.Design
    QML_FILES
        Theme.qml)

qt_add_resources(moligram_design brand
    PREFIX /moligram
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/branding/logo.png"
        "${PROJECT_SOURCE_DIR}/resources/textures/grain.png")
