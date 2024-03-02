cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME high-performance-hair-rendering)

project(${EXECUTABLE_NAME})

# Using C++20
# -----------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OS-specific needs.
# -----------------------

if (APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    add_library(MetalUtility SHARED "external/MetalUtility/MetalUtility.m")
    target_link_libraries(MetalUtility ${QUARTZ_CORE} ${COCOA_LIBRARY} glfw)
endif ()

# Packages.
# -----------------------

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(cxxopts REQUIRED)

add_executable(${EXECUTABLE_NAME} source/main.cpp source/shader.cpp source/swapchain.cpp)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${Vulkan_LIBRARIES} glfw MetalUtility)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()