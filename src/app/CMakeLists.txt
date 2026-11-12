qt_add_executable(moligram main.cpp)

moligram_enable_warnings(moligram)
target_compile_definitions(moligram PRIVATE MOLIGRAM_VERSION="${PROJECT_VERSION}")

target_link_libraries(moligram PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    moligram_designplugin
    moligram_controlsplugin
    moligram_screensplugin)

install(TARGETS moligram RUNTIME DESTINATION bin)
