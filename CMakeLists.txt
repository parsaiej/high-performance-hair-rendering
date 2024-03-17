cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_NAME high-performance-hair-rendering)

project(${EXECUTABLE_NAME})

set (CMAKE_PREFIX_PATH "~/Development/OpenUSD_Build/")

# Using C++20
# -----------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Packages.
# -----------------------

find_package(pxr REQUIRED)
find_package(cxxopts REQUIRED)
find_package(glm REQUIRED)
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

target_include_directories(${EXECUTABLE_NAME} PRIVATE 
    ${Vulkan_INCLUDE_DIRS} 
    ${PXR_INCLUDE_DIRS} 
    ${glm_INCLUDE_DIRS} 
    "external/VK-Demo-Wrappers/"
)

# Link
# -----------------------

target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
    ${Vulkan_LIBRARIES} 
    ${PXR_LIBRARIES} 
    ${glm_LIBRARIES}
    cxxopts::cxxopts
    Wrappers
)

# Defines
# -----------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()