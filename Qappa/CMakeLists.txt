cmake_minimum_required(VERSION 3.5)

project(Qappa LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set_property(SOURCE Qappaplugin_QappaPlugin.moc PROPERTY SKIP_AUTOGEN ON)

qt6_add_qml_module(Qappa
    URI Qappa
    VERSION 1.0
    SOURCES
        UXManager.hpp
        UXManager.cpp
        ComponentManager.cpp
        ComponentManager.hpp
    QML_FILES
        Blanks/HorizontalTwoPanel.qml
        Panel.qml
        Blank.qml
)

