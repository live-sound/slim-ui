cmake_minimum_required(VERSION 3.20)
project(SlimUiNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(SlimUi.Native SHARED
    src/slim_ui_net.cpp
)

add_library(SlimUi.Sample.Native SHARED
    src/slim_ui_net.cpp
    ../../demo/shader_effect.cpp
)
target_compile_definitions(SlimUi.Sample.Native PRIVATE SUI_NET_SAMPLE_SHADER)

include(CTest)
if(BUILD_TESTING)
    add_executable(SlimUi.Native.Tests tests/window_tests.cpp ../../demo/shader_effect.cpp)
    target_compile_definitions(SlimUi.Native.Tests PRIVATE SUI_NET_SAMPLE_SHADER)
    add_test(NAME SlimUi.Native.Tests COMMAND SlimUi.Native.Tests)
endif()

set(bridge_targets SlimUi.Native SlimUi.Sample.Native)
if(BUILD_TESTING)
    list(APPEND bridge_targets SlimUi.Native.Tests)
endif()
foreach(target IN LISTS bridge_targets)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )

    target_compile_definitions(${target} PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
        $<$<CXX_COMPILER_ID:MSVC>:/W3>
    )

    target_link_libraries(${target} PRIVATE
        d2d1
        dwrite
        user32
        gdi32
        ole32
        shell32
        windowscodecs
    )
endforeach()
