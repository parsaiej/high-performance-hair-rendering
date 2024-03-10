cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME high-performance-hair-rendering)

project(${EXECUTABLE_NAME})

# Using C++20
# -----------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Packages.
# -----------------------

find_package(Vulkan REQUIRED)

# Simple VK-primitive Wrappers
# -----------------------

add_subdirectory("external/VK-Demo-Wrappers/")

# Implementation Executable
# -----------------------

add_executable(${EXECUTABLE_NAME} 
    source/main.cpp 
)

# Include
# -----------------------

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS} "external/")

# Link
# -----------------------

target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${Vulkan_LIBRARIES} Wrappers)

# Defines
# -----------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()