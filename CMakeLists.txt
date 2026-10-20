cmake_minimum_required(VERSION 3.21)

project(Resources
    VERSION 1.3.1
    DESCRIPTION "Reusable Qt resources, theme, and window chrome."
    LANGUAGES CXX
)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(RESOURCES_BUILD_WIDGET_TESTS
    "Build reusable Qt widget behavior and geometry tests." OFF)
option(RESOURCES_BUILD_THEME "Build resource assets and window chrome." ON)
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/Resources>)
set_target_properties(ResourcesWidgets PROPERTIES EXPORT_NAME Widgets
    VERSION ${PROJECT_VERSION} POSITION_INDEPENDENT_CODE ON)
install(TARGETS ResourcesWidgets EXPORT ResourcesWidgetsTargets
    ARCHIVE DESTINATION lib COMPONENT PluginSDK EXCLUDE_FROM_ALL)
install(FILES Widgets/SearchableTreeWidget.h Widgets/SearchableTreeController.h
    DESTINATION include/Resources/Widgets COMPONENT PluginSDK EXCLUDE_FROM_ALL)
install(EXPORT ResourcesWidgetsTargets NAMESPACE Resources::
    DESTINATION lib/cmake/ResourcesWidgets COMPONENT PluginSDK EXCLUDE_FROM_ALL)
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/ResourcesWidgetsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ResourcesWidgetsConfig.cmake"
    INSTALL_DESTINATION lib/cmake/ResourcesWidgets)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ResourcesWidgetsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ResourcesWidgetsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ResourcesWidgetsConfigVersion.cmake"
    DESTINATION lib/cmake/ResourcesWidgets COMPONENT PluginSDK EXCLUDE_FROM_ALL)
qt_add_library(ResourcesDialogs STATIC
    Chrome/ThemedDialog.h Chrome/ThemedDialog.cpp
    Chrome/ThemedFileDialog.h Chrome/ThemedFileDialog.cpp
    Chrome/ThemedMessageBox.h Chrome/ThemedMessageBox.cpp
    Chrome/WindowPlatformAttributes.h Chrome/WindowPlatformAttributes.cpp)
add_library(Resources::Dialogs ALIAS ResourcesDialogs)
set_target_properties(ResourcesDialogs PROPERTIES EXPORT_NAME Dialogs
    POSITION_INDEPENDENT_CODE ON)
target_compile_features(ResourcesDialogs PUBLIC cxx_std_17)
target_include_directories(ResourcesDialogs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include/Resources>)
target_link_libraries(ResourcesDialogs PUBLIC Qt6::Widgets
    PRIVATE $<$<PLATFORM_ID:Windows>:dwmapi>)
install(TARGETS ResourcesDialogs EXPORT ResourcesWidgetsTargets
    ARCHIVE DESTINATION lib COMPONENT PluginSDK EXCLUDE_FROM_ALL)
install(FILES Chrome/ThemedDialog.h Chrome/ThemedFileDialog.h Chrome/ThemedMessageBox.h
    Chrome/WindowPlatformAttributes.h DESTINATION include/Resources/Chrome
    COMPONENT PluginSDK EXCLUDE_FROM_ALL)
if(NOT RESOURCES_BUILD_THEME)
    return()
endif()

qt_add_library(PlaygroundResources STATIC
    Resources.qrc
    Resources.h
    Resources.cpp
    ItemViewTheme.h ItemViewTheme.cpp
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
        Resources::Dialogs
    PRIVATE
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

target_include_directories(PlaygroundResources
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(RESOURCES_BUILD_TRANSIENT_WINDOW_TESTS)
    qt_add_executable(ResourcesItemViewTests tests/ResourcesItemViewTests.cpp)
    target_link_libraries(ResourcesItemViewTests PRIVATE Resources::Resources Qt6::Test)
    set_target_properties(ResourcesItemViewTests PROPERTIES WIN32_EXECUTABLE FALSE)
    if(WIN32)
        add_test(NAME ResourcesItemViewTests COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}" "QT_QPA_PLATFORM=offscreen"
            $<TARGET_FILE:ResourcesItemViewTests>)
    else()
        add_test(NAME ResourcesItemViewTests COMMAND ResourcesItemViewTests)
        set_tests_properties(ResourcesItemViewTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
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
