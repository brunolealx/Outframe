cmake_minimum_required(VERSION 3.24)

project(FPSgradeEngine
    VERSION 0.1.0
    DESCRIPTION "Clean-room Windows upscaling and presentation engine for games"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(FPSgradeEngine WIN32
    src/main.cpp
    src/app/FpsgradeApp.cpp
    src/app/FpsgradeApp.h
    src/platform/Win32Window.cpp
    src/platform/Win32Window.h
)

target_compile_definitions(FPSgradeEngine PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(FPSgradeEngine PRIVATE
    src
)

target_link_libraries(FPSgradeEngine PRIVATE
    user32
    gdi32
    dwmapi
)

if(MSVC)
    target_compile_options(FPSgradeEngine PRIVATE /W4 /permissive-)
endif()
