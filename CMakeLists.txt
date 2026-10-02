cmake_minimum_required(VERSION 3.21)

project(Resources
    VERSION 0.1.0
    DESCRIPTION "Reusable Qt resources, theme, and window chrome."
    LANGUAGES CXX
)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(RESOURCES_BUILD_WIDGET_TESTS
    "Build reusable Qt widget behavior and geometry tests." OFF)
option(RESOURCES_BUILD_MDI_SHADOW_TESTS
    "Build focused MDI shadow lifecycle and stacking tests." OFF)
option(RESOURCES_BUILD_TRANSIENT_WINDOW_TESTS
    "Build focused loading-window, menu, and combo-popup lifecycle tests." OFF)
if(RESOURCES_BUILD_WIDGET_TESTS OR RESOURCES_BUILD_MDI_SHADOW_TESTS OR RESOURCES_BUILD_TRANSIENT_WINDOW_TESTS)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

# This target uses only Qt; it does not link assets, install themes, or own device behavior.
qt_add_library(ResourcesWidgets STATIC
    Widgets/SearchableTreeWidget.h
    Widgets/SearchableTreeWidget.cpp
    Widgets/SearchableTreeController.h
    Widgets/SearchableTreeController.cpp)
add_library(Resources::Widgets ALIAS ResourcesWidgets)
target_compile_features(ResourcesWidgets PUBLIC cxx_std_17)
target_link_libraries(ResourcesWidgets PUBLIC Qt6::Widgets)
target_include_directories(ResourcesWidgets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

qt_add_library(PlaygroundResources STATIC
    Resources.qrc
    Resources.h
    Resources.cpp
    Chrome/ThemedTreeWidget.h
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
        Resources::Widgets
    PRIVATE
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

target_include_directories(PlaygroundResources
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(RESOURCES_BUILD_TRANSIENT_WINDOW_TESTS)
    qt_add_executable(ResourcesTransientWindowTests tests/ResourcesTransientWindowTests.cpp)
    target_link_libraries(ResourcesTransientWindowTests PRIVATE Resources::Resources Qt6::Test)
    set_target_properties(ResourcesTransientWindowTests PROPERTIES WIN32_EXECUTABLE FALSE)
    if(WIN32)
        add_test(NAME ResourcesTransientWindowTests COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}" "QT_QPA_PLATFORM=offscreen"
            $<TARGET_FILE:ResourcesTransientWindowTests>)
    else()
        add_test(NAME ResourcesTransientWindowTests COMMAND ResourcesTransientWindowTests)
        set_tests_properties(ResourcesTransientWindowTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

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

if(RESOURCES_BUILD_WIDGET_TESTS)
    qt_add_executable(ResourcesWidgetTests tests/SearchableTreeWidgetTests.cpp)
    target_link_libraries(ResourcesWidgetTests PRIVATE Resources::Widgets Qt6::Test)
    set_target_properties(ResourcesWidgetTests PROPERTIES WIN32_EXECUTABLE FALSE)
    if(WIN32)
        add_test(NAME ResourcesWidgetTests COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}" "QT_QPA_PLATFORM=offscreen"
            $<TARGET_FILE:ResourcesWidgetTests>)
    else()
        add_test(NAME ResourcesWidgetTests COMMAND ResourcesWidgetTests)
        set_tests_properties(ResourcesWidgetTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
