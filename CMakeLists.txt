cmake_minimum_required(VERSION 3.21)

project(Resources
    VERSION 0.1.0
    DESCRIPTION "Shared Qt resource bundle for Playground modules."
    LANGUAGES CXX
)

set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

qt_add_library(PlaygroundResources STATIC
    Resources.qrc
    Resources.h
    Resources.cpp
    Chrome/CustomTitleBar.h
    Chrome/CustomTitleBar.cpp
    Chrome/DockTitleBar.h
    Chrome/DockTitleBar.cpp
    Chrome/MainTitleBar.h
    Chrome/MainTitleBar.cpp
    Chrome/MdiSubWindowContainer.h
    Chrome/MdiSubWindowContainer.cpp
    Chrome/ThemedDialog.h
    Chrome/ThemedDialog.cpp
    Chrome/ThemedMessageBox.h
    Chrome/ThemedMessageBox.cpp
    Chrome/SplashWidget.h
    Chrome/SplashWidget.cpp
    Chrome/LoadingWidget.h
    Chrome/LoadingWidget.cpp
    Chrome/ProgressItem.h
)

add_library(Resources::Resources ALIAS PlaygroundResources)

target_link_libraries(PlaygroundResources
    PUBLIC
        Qt6::Widgets
)

target_include_directories(PlaygroundResources
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
