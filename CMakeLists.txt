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
    Chrome/ThemedMdiTitleBar.h
    Chrome/ThemedMdiTitleBar.cpp
    Chrome/ThemedDockTitleBar.h
    Chrome/ThemedDockTitleBar.cpp
    Chrome/ThemedMainTitleBar.h
    Chrome/ThemedMainTitleBar.cpp
    Chrome/ThemedMdiContainer.h
    Chrome/ThemedMdiContainer.cpp
    Chrome/ThemedDialog.h
    Chrome/ThemedDialog.cpp
    Chrome/ThemedMessageBox.h
    Chrome/ThemedMessageBox.cpp
    Chrome/ThemedSplashWidget.h
    Chrome/ThemedSplashWidget.cpp
    Chrome/ThemedLoadingWidget.h
    Chrome/ThemedLoadingWidget.cpp
    Chrome/ThemedProgressBar.h
    Chrome/ThemedWindow.h
    Chrome/ThemedWindow.cpp
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
