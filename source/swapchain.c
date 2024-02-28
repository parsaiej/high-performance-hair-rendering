#include "swapchain.h"

#include <GLFW/glfw3.h>
#include <stdlib.h>

typedef struct 
{
    VkSurfaceCapabilitiesKHR capabilities;

    uint32_t                 formatsCount;
    VkSurfaceFormatKHR*      formats;

    uint32_t                 presentModesCount;
    VkPresentModeKHR*        presentModes;

} SwapChainSupportDetails;

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);

    if (formatCount != 0) 
    {
        details.formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats);
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);

    if (presentModeCount != 0)
    {
        details.presentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes);
    }

    return details;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(SwapChainSupportDetails swapChainInfo)
{
    for (uint32_t i = 0; i < swapChainInfo.formatsCount; i++) 
    {
        if (swapChainInfo.formats[i].format     == VK_FORMAT_B8G8R8A8_SRGB && 
            swapChainInfo.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return swapChainInfo.formats[i];
    }

    return swapChainInfo.formats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(SwapChainSupportDetails swapChainInfo)
{
    for (int i = 0; i < swapChainInfo.presentModesCount; i++) 
    {
        if (swapChainInfo.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return swapChainInfo.presentModes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

uint32_t Clamp(uint32_t val, uint32_t min, uint32_t max) 
{
    if      (val < min) return min;
    else if (val > max) return max;
    else return val;
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities, GLFWwindow* window)
{
    if ((*capabilities).currentExtent.width != 0xFFFFFFFFu)
        return (*capabilities).currentExtent;
    else 
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            (uint32_t)(width),
            (uint32_t)(height)
        };

        actualExtent.width  = Clamp(actualExtent.width,  (*capabilities).minImageExtent.width,  (*capabilities).maxImageExtent.width);
        actualExtent.height = Clamp(actualExtent.height, (*capabilities).minImageExtent.height, (*capabilities).maxImageExtent.height);

        return actualExtent;
    }
}

VkResult CreateSwapChain(SwapChainParams params, SwapChain* swapChain)
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(params.physicalDevice, params.surface);

    // Store the format and extent
    swapChain->format = ChooseSwapSurfaceFormat(swapChainSupport);
    swapChain->extent = ChooseSwapExtent(&swapChainSupport.capabilities, NULL);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo =
    {
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface               = params.surface,
        .minImageCount         = imageCount,
        .imageExtent           = swapChain->extent,
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform          = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = ChooseSwapPresentMode(swapChainSupport),
        .clipped               = VK_TRUE,
        .oldSwapchain          = VK_NULL_HANDLE,
        .imageFormat           = swapChain->format.format,
        .imageColorSpace       = swapChain->format.colorSpace,
        .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = NULL
    };

    // Release the swap chain info.

    free(swapChainSupport.formats);
    free(swapChainSupport.presentModes);

    VkResult swapChainCreateResult = vkCreateSwapchainKHR(*params.device, &createInfo, NULL, &swapChain->primitive);

    if (swapChainCreateResult != VK_SUCCESS)
        return swapChainCreateResult;

    // Fetch image count.

    vkGetSwapchainImagesKHR(*params.device, swapChain->primitive, &swapChain->imageCount, NULL);
    
    swapChain->images = (VkImage*)malloc(sizeof(VkImage) * swapChain->imageCount);
    vkGetSwapchainImagesKHR(*params.device, swapChain->primitive, &swapChain->imageCount, swapChain->images);

    // Create image views.

    swapChain->imageViews = (VkImageView*)malloc(sizeof(VkImageView*) * swapChain->imageCount);

    for (size_t i = 0; i < swapChain->imageCount; ++i)
    {
        VkImageViewCreateInfo createInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image        = swapChain->images[i],
            .viewType     = VK_IMAGE_VIEW_TYPE_2D,
            .format       = swapChain->format.format,

            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,

            .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel   = 0,
            .subresourceRange.levelCount     = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount     = 1
        };

        VkResult imageViewCreateResult = vkCreateImageView(*params.device, &createInfo, NULL, &swapChain->imageViews[i]);

        if (imageViewCreateResult != VK_SUCCESS)
            return imageViewCreateResult;
    }

    return VK_SUCCESS;
}

void ReleaseSwapChain(VkDevice device, SwapChain* swapChain)
{
    for (size_t i = 0; i < swapChain->imageCount; ++i)
        vkDestroyImageView(device, swapChain->imageViews[i], NULL);

    free(swapChain->imageViews);
    free(swapChain->images);

    vkDestroySwapchainKHR(device, swapChain->primitive, NULL);
}