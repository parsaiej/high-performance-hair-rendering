#include "swapchain.h"

#include <GLFW/glfw3.h>

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);

    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);

    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());

    return details;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(SwapChainSupportDetails swapChainInfo)
{
    for (auto format : swapChainInfo.formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }

    return swapChainInfo.formats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(SwapChainSupportDetails swapChainInfo)
{
    for (auto presentMode : swapChainInfo.presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
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

void TryCreateSwapChain(SwapChainParams params, SwapChain* swapChain)
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

    if(vkCreateSwapchainKHR(*params.device, &createInfo, NULL, &swapChain->primitive) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain.");

    // Fetch image count.

    vkGetSwapchainImagesKHR(*params.device, swapChain->primitive, &swapChain->imageCount, NULL);
    
    swapChain->images.resize(swapChain->imageCount);
    vkGetSwapchainImagesKHR(*params.device, swapChain->primitive, &swapChain->imageCount, swapChain->images.data());

    // Create image views.
    swapChain->imageViews.resize(swapChain->imageCount);

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

        if (vkCreateImageView(*params.device, &createInfo, NULL, &swapChain->imageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain image view.");
    }

    // Create synchronization primitives. 
    {
        VkSemaphoreCreateInfo semaphoreInfo { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        
        VkFenceCreateInfo fenceInfo
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,

            // Create in a signaled state. 
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        if (vkCreateSemaphore(*params.device, &semaphoreInfo, nullptr, &swapChain->imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(*params.device, &semaphoreInfo, nullptr, &swapChain->renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence    (*params.device, &fenceInfo,     nullptr, &swapChain->inFlightFence)           != VK_SUCCESS)
            throw std::runtime_error("failed to create swapchain synchronziation primitives.");
    }
}

void ReleaseSwapChain(VkDevice device, SwapChain* swapChain)
{
    vkDestroySemaphore(device, swapChain->imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(device, swapChain->renderFinishedSemaphore, nullptr);

    vkDestroyFence(device, swapChain->inFlightFence, nullptr);

    for (size_t i = 0; i < swapChain->imageCount; ++i)
        vkDestroyImageView(device, swapChain->imageViews[i], NULL);

    vkDestroySwapchainKHR(device, swapChain->primitive, NULL);
}