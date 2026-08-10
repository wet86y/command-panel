cmake_minimum_required(VERSION 3.24)

project(CommandPanel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CommandPanel WIN32
    src/main.cpp
    src/App.cpp
    src/MainWindow.cpp
    src/ButtonPanel.cpp
    src/TabBar.cpp
    src/CommandDialog.cpp
    src/ConfigManager.cpp
    src/TerminalSession.cpp
    src/TerminalParser.cpp
    src/CommandExecutor.cpp
    src/Utf.cpp
    resources/app.rc
)

target_include_directories(CommandPanel PRIVATE src)

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
)

if(MSVC)
    target_link_options(CommandPanel PRIVATE
        /MANIFEST:NO
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()
