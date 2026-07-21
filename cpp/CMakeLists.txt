cmake_minimum_required(VERSION 3.10)

project(MatchAndBeautyCore)

# Add our C++ source files
add_library(
    match_and_beauty_core
    SHARED
    MatchAndBeautyCore.cpp
    FizgravityJNI.cpp
    FizgravityRenderer.cpp
)

# Specify include directories
target_include_directories(
    match_and_beauty_core
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link log and android native graphics library (for AHardwareBuffer, etc.)
# dl is required for dlopen/dlsym to dynamically load libfizgravity_ar.so at runtime
target_link_libraries(
    match_and_beauty_core
    android
    log
    dl
    EGL
    GLESv2
)
