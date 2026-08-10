cmake_minimum_required(VERSION 3.21)

project(Resources
    VERSION 0.1.0
    DESCRIPTION "Shared Qt resource bundle for Playground modules."
    LANGUAGES CXX
)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(RESOURCES_BUILD_MDI_SHADOW_TESTS
    "Build focused MDI shadow lifecycle and stacking tests." OFF)
if(RESOURCES_BUILD_MDI_SHADOW_TESTS)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()
endif()

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
    Chrome/ThemedMdiArea.h
    Chrome/ThemedMdiArea.cpp
    Chrome/ThemedDialog.h
    Chrome/ThemedDialog.cpp
    Chrome/ThemedFileDialog.h
    Chrome/ThemedFileDialog.cpp
    Chrome/ThemedMessageBox.h
    Chrome/ThemedMessageBox.cpp
    Chrome/ThemedSplashWidget.h
    Chrome/ThemedSplashWidget.cpp
    Chrome/ThemedLoadingWidget.h
    Chrome/ThemedLoadingWidget.cpp
    Chrome/ThemedProgressBar.h
    Chrome/ThemedMainWindow.h
    Chrome/ThemedMainWindow.cpp
)

add_library(Resources::Resources ALIAS PlaygroundResources)

target_compile_features(PlaygroundResources PUBLIC cxx_std_17)

target_link_libraries(PlaygroundResources
    PUBLIC
        Qt6::Widgets
    PRIVATE
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

target_include_directories(PlaygroundResources
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(RESOURCES_BUILD_MDI_SHADOW_TESTS)
    qt_add_executable(ResourcesMdiShadowTests
        tests/ResourcesMdiShadowTests.cpp
    )
    target_link_libraries(ResourcesMdiShadowTests
        PRIVATE
            Resources::Resources
            Qt6::Test
            Qt6::Widgets
    )
    set_target_properties(ResourcesMdiShadowTests PROPERTIES WIN32_EXECUTABLE FALSE)
    if(WIN32)
        add_test(NAME ResourcesMdiShadowTests
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
                "QT_QPA_PLATFORM=offscreen"
                $<TARGET_FILE:ResourcesMdiShadowTests> -o -,txt)
    else()
        add_test(NAME ResourcesMdiShadowTests
            COMMAND ResourcesMdiShadowTests -o -,txt)
        set_tests_properties(ResourcesMdiShadowTests
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
