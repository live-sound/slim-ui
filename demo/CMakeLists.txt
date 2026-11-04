cmake_minimum_required(VERSION 3.20)
project(sui_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sui_demo WIN32 main.cpp sui_demo.rc)

target_include_directories(sui_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_options(sui_demo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(sui_demo PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(sui_demo PRIVATE
    d2d1
    dwrite
    user32
    gdi32
    ole32
    shell32
    windowscodecs
)

set_target_properties(sui_demo PROPERTIES
    VS_DPI_AWARE "PerMonitor"
)
