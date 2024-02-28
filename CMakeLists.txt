cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME high-performance-hair-rendering)

project(${EXECUTABLE_NAME} C)

set(CMAKE_C_STANDARD 11)

if (APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    add_library(MetalUtility SHARED "external/MetalUtility/MetalUtility.m")
    target_link_libraries(MetalUtility ${QUARTZ_CORE} ${COCOA_LIBRARY} glfw)
endif ()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(${EXECUTABLE_NAME} source/main.c source/shader.c source/swapchain.c)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${Vulkan_LIBRARIES} glfw MetalUtility)