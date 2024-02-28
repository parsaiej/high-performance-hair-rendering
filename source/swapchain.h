#ifndef SWAPCHAIN
#define SWAPCHAIN

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct
{
    GLFWwindow*       window;
    VkDevice*         device;
    VkSurfaceKHR      surface;
    VkPhysicalDevice  physicalDevice;
    uint32_t          queueIndexGraphics;
    uint32_t          queueIndexPresent;

} SwapChainParams;

typedef struct
{
    VkSwapchainKHR     primitive;
    uint32_t           imageCount;
    VkImage*           images;
    VkImageView*       imageViews;
    VkSurfaceFormatKHR format;
    VkExtent2D         extent;

} SwapChain;

VkResult CreateSwapChain(SwapChainParams params, SwapChain* swapchain);

void ReleaseSwapChain(VkDevice device, SwapChain* swapchain);

#endif//SWAPCHAIN