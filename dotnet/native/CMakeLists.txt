cmake_minimum_required(VERSION 3.20)
project(SlimUiNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(SlimUi.Native SHARED
    src/slim_ui_net.cpp
)

target_include_directories(SlimUi.Native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_compile_definitions(SlimUi.Native PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(SlimUi.Native PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
)

target_link_libraries(SlimUi.Native PRIVATE
    d2d1
    dwrite
    user32
    gdi32
    ole32
    shell32
    windowscodecs
)
