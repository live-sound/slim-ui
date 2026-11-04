cmake_minimum_required(VERSION 3.21)
project(slim_ui LANGUAGES C CXX)

# Header-only: the target carries the include path and the Windows libraries the toolkit uses.
add_library(slim_ui INTERFACE)
target_include_directories(slim_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(slim_ui INTERFACE d2d1 dwrite dwmapi user32 gdi32 ole32 shell32 windowscodecs)

option(SLIM_UI_BUILD_DEMO "Build the demo" ${PROJECT_IS_TOP_LEVEL})
option(SLIM_UI_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
if(SLIM_UI_BUILD_DEMO)
    add_subdirectory(demo)
endif()
if(SLIM_UI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
