#ifndef SWAPCHAIN
#define SWAPCHAIN

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

struct SwapChainParams
{
    GLFWwindow*       window;
    VkDevice*         device;
    VkSurfaceKHR      surface;
    VkPhysicalDevice  physicalDevice;
    uint32_t          queueIndexGraphics;
    uint32_t          queueIndexPresent;
};

struct SwapChain
{
    VkSwapchainKHR           primitive;
    uint32_t                 imageCount;
    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;
    VkSurfaceFormatKHR       format;
    VkExtent2D               extent;
    VkSemaphore              imageAvailableSemaphore;
    VkSemaphore              renderFinishedSemaphore;
    VkFence                  inFlightFence;
};

void TryCreateSwapChain(SwapChainParams params, SwapChain* swapchain);

void ReleaseSwapChain(VkDevice device, SwapChain* swapchain);

#endif//SWAPCHAIN