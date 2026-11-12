cmake_minimum_required(VERSION 3.28)

project(Moligram
    VERSION 0.1.0
    DESCRIPTION "Desktop Telegram client"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
set(QT_QML_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml")

add_subdirectory(src)
