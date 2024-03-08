
add_library(Wrappers SHARED "RenderInstance.cpp")

target_include_directories(Wrappers PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/external/)

target_link_libraries(Wrappers PRIVATE ${Vulkan_LIBRARIES} glfw MetalUtility)