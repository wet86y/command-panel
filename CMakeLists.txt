cmake_minimum_required(VERSION 3.25)

project(CommandPanel VERSION 1.0.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/DesktopUpdateKit/native"
    "${CMAKE_BINARY_DIR}/desktop-update-kit")

set(COMMAND_PANEL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${COMMAND_PANEL_GENERATED_DIR}")
configure_file(src/Version.h.in "${COMMAND_PANEL_GENERATED_DIR}/Version.h" @ONLY)
configure_file(resources/app.manifest "${COMMAND_PANEL_GENERATED_DIR}/app.manifest" COPYONLY)
configure_file(resources/app.ico "${COMMAND_PANEL_GENERATED_DIR}/app.ico" COPYONLY)
configure_file(resources/app.rc.in "${COMMAND_PANEL_GENERATED_DIR}/app.rc" @ONLY)
file(GENERATE OUTPUT "${COMMAND_PANEL_GENERATED_DIR}/$<CONFIG>/updater_stub.rc"
    CONTENT "201 RCDATA \"bin/$<CONFIG>/DesktopUpdateKitUpdaterStub.exe\"\n")

add_executable(CommandPanel WIN32
    src/main.cpp
    src/App.cpp
    src/MainWindow.cpp
    src/ButtonPanel.cpp
    src/ButtonLayout.cpp
    src/TabBar.cpp
    src/CommandDialog.cpp
    src/CommandMenu.cpp
    src/ConfigManager.cpp
    src/TerminalSession.cpp
    src/TerminalParser.cpp
    src/TerminalModel.cpp
    src/TerminalView.cpp
    src/CommandExecutor.cpp
    src/AboutLayout.cpp
    src/AboutButton.cpp
    src/AboutWindow.cpp
    src/ExecutableNameNormalizer.cpp
    src/UpdateCoordinator.cpp
    src/Utf.cpp
    src/UiTheme.cpp
    src/UiScroll.cpp
    "${COMMAND_PANEL_GENERATED_DIR}/app.rc"
    "${COMMAND_PANEL_GENERATED_DIR}/$<CONFIG>/updater_stub.rc"
)

set_target_properties(CommandPanel PROPERTIES
    OUTPUT_NAME "快捷控制台"
    PROJECT_LABEL "快捷控制台"
)

target_include_directories(CommandPanel PRIVATE src "${COMMAND_PANEL_GENERATED_DIR}")

target_compile_definitions(CommandPanel PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    NOMINMAX
)

target_compile_options(CommandPanel PRIVATE
    /utf-8
    /W4
    /permissive-
    /EHsc
)

set_property(TARGET CommandPanel PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(CommandPanel PRIVATE
    user32
    gdi32
    comctl32
    shell32
    advapi32
    ole32
    crypt32
    dwmapi
    gdiplus
    uxtheme
    desktop_update_kit_native
)

if(MSVC)
    target_link_options(CommandPanel PRIVATE
        /MANIFEST:NO
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(CommandPanelUiTests
        tests/UiScrollTests.cpp
        src/UiScroll.cpp
        src/ButtonLayout.cpp
        src/UiTheme.cpp
        src/TerminalModel.cpp
        src/TerminalParser.cpp
        src/ConfigManager.cpp
        src/Utf.cpp
        src/AboutLayout.cpp
        src/AboutButton.cpp
        src/ExecutableNameNormalizer.cpp
    )
    target_include_directories(CommandPanelUiTests PRIVATE src "${COMMAND_PANEL_GENERATED_DIR}")
    target_compile_definitions(CommandPanelUiTests PRIVATE
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
        NOMINMAX
    )
    target_compile_options(CommandPanelUiTests PRIVATE /utf-8 /W4 /permissive-)
    set_property(TARGET CommandPanelUiTests PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(CommandPanelUiTests PRIVATE user32 gdi32 comctl32 dwmapi crypt32 shell32 ole32 desktop_update_kit_native)
    add_test(NAME CommandPanelUiTests COMMAND CommandPanelUiTests)
endif()
