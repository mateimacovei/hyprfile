cmake_minimum_required(VERSION 3.10)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(hyprfile_tests LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    if(NOT DEFINED BUILD_PROFILE)
        set(BUILD_PROFILE "Debug" CACHE STRING "Build profile for tests (Debug/Release)")
    endif()

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "${BUILD_PROFILE}" CACHE STRING "Build type for tests" FORCE)
    endif()

    string(TOUPPER "${CMAKE_BUILD_TYPE}" _TEST_BUILD_PROFILE_UPPER)
    if(_TEST_BUILD_PROFILE_UPPER STREQUAL "DEBUG")
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type for tests" FORCE)
    elseif(_TEST_BUILD_PROFILE_UPPER STREQUAL "RELEASE")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type for tests" FORCE)
    endif()

    message(STATUS "Tests build profile: ${CMAKE_BUILD_TYPE}")
endif()

get_filename_component(HYPRFILE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(HYPRFILE_SRC "${HYPRFILE_ROOT}/src")

find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)

if(NOT TARGET PkgConfig::HYPR)
    pkg_check_modules(HYPR REQUIRED IMPORTED_TARGET hyprtoolkit hyprutils pixman-1 aquamarine libdrm)
endif()

if(NOT TARGET PkgConfig::XKB)
    pkg_check_modules(XKB REQUIRED IMPORTED_TARGET xkbcommon)
endif()

enable_testing()
include(GoogleTest)

add_test(
    NAME release_script_tests
    COMMAND bash "${CMAKE_CURRENT_LIST_DIR}/ReleaseScriptTests.sh" "${HYPRFILE_ROOT}"
)

function(hyprfile_add_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE "${HYPRFILE_SRC}" "${HYPRFILE_GENERATED_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${target})
endfunction()

hyprfile_add_test(parser_tests
    ParserTests.cpp
    "${HYPRFILE_SRC}/cli/Parser.cpp"
)
target_link_libraries(parser_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(debug_report_tests
    DebugReportTests.cpp
    "${HYPRFILE_SRC}/debug/DependencyReport.cpp"
)

hyprfile_add_test(keymap_tests
    KeyBindingsTests.cpp
    "${HYPRFILE_SRC}/core/KeyBindings.cpp"
)
target_link_libraries(keymap_tests PRIVATE PkgConfig::XKB)

hyprfile_add_test(file_operation_tests
    FileOperationTests.cpp
    "${HYPRFILE_SRC}/core/FileOperationClipboard.cpp"
    "${HYPRFILE_SRC}/core/FileOperations.cpp"
)

hyprfile_add_test(search_input_tests
    SearchInputTests.cpp
)
target_link_libraries(search_input_tests PRIVATE PkgConfig::HYPR PkgConfig::XKB)

hyprfile_add_test(application_layout_tests
    ApplicationLayoutTests.cpp
)
target_link_libraries(application_layout_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(help_popup_tests
    HelpPopupTests.cpp
    "${HYPRFILE_SRC}/core/KeyBindings.cpp"
    "${HYPRFILE_SRC}/ui/HelpPopup.cpp"
)
target_link_libraries(help_popup_tests PRIVATE PkgConfig::HYPR PkgConfig::XKB)

hyprfile_add_test(status_bar_tests
    StatusBarTests.cpp
    "${HYPRFILE_SRC}/ui/StatusBar.cpp"
    "${HYPRFILE_SRC}/ui/model/FileItem.cpp"
)
target_link_libraries(status_bar_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(top_bar_tests
    TopBarTests.cpp
    "${HYPRFILE_SRC}/ui/TopBar.cpp"
)
target_link_libraries(top_bar_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(terminal_launcher_tests
    TerminalLauncherTests.cpp
    "${HYPRFILE_SRC}/core/TerminalLauncher.cpp"
    "${HYPRFILE_SRC}/core/ProcessLauncher.cpp"
)

hyprfile_add_test(mount_info_service_tests
    MountInfoServiceTests.cpp
    "${HYPRFILE_SRC}/core/MountInfoService.cpp"
)

hyprfile_add_test(mount_info_column_tests
    MountInfoColumnTests.cpp
    "${HYPRFILE_SRC}/core/MountInfoService.cpp"
    "${HYPRFILE_SRC}/ui/columns/layout/MountInfoColumn.cpp"
)
target_link_libraries(mount_info_column_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(video_thumbnail_seek_tests
    VideoThumbnailSeekTests.cpp
)

hyprfile_add_test(directory_column_tests
    DirectoryColumnTests.cpp
    "${HYPRFILE_SRC}/core/FileOperationClipboard.cpp"
    "${HYPRFILE_SRC}/core/FileOperations.cpp"
    "${HYPRFILE_SRC}/core/FileSystemService.cpp"
    "${HYPRFILE_SRC}/core/ProcessLauncher.cpp"
    "${HYPRFILE_SRC}/core/TerminalLauncher.cpp"
    "${HYPRFILE_SRC}/ui/columns/layout/DirectoryColumn.cpp"
    "${HYPRFILE_SRC}/ui/model/FileItem.cpp"
    "${HYPRFILE_SRC}/ui/model/FileItemLayout.cpp"
)
target_link_libraries(directory_column_tests PRIVATE PkgConfig::HYPR)
target_compile_definitions(directory_column_tests PRIVATE _GLIBCXX_ASSERTIONS)

hyprfile_add_test(parent_column_tests
    ParentColumnTests.cpp
    "${HYPRFILE_SRC}/core/FileOperationClipboard.cpp"
    "${HYPRFILE_SRC}/core/FileOperations.cpp"
    "${HYPRFILE_SRC}/core/FileSystemService.cpp"
    "${HYPRFILE_SRC}/core/MountInfoService.cpp"
    "${HYPRFILE_SRC}/core/ProcessLauncher.cpp"
    "${HYPRFILE_SRC}/core/TerminalLauncher.cpp"
    "${HYPRFILE_SRC}/ui/columns/ParentColumn.cpp"
    "${HYPRFILE_SRC}/ui/columns/layout/DirectoryColumn.cpp"
    "${HYPRFILE_SRC}/ui/columns/layout/MountInfoColumn.cpp"
    "${HYPRFILE_SRC}/ui/model/FileItem.cpp"
    "${HYPRFILE_SRC}/ui/model/FileItemLayout.cpp"
)
target_link_libraries(parent_column_tests PRIVATE PkgConfig::HYPR)

hyprfile_add_test(preview_text_column_tests
    PreviewTextColumnTests.cpp
)
target_link_libraries(preview_text_column_tests PRIVATE PkgConfig::HYPR)
