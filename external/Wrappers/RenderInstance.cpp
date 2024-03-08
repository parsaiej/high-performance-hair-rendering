#include "RenderInstance.h"

#include <vector>
#include <iostream>

using namespace Wrappers;

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

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities, GLFWwindow* window)
{
    if ((*capabilities).currentExtent.width != 0xFFFFFFFFu)
        return (*capabilities).currentExtent;
    else 
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = 
        {
            (uint32_t)(width),
            (uint32_t)(height)
        };

        actualExtent.width  = std::clamp(actualExtent.width,  (*capabilities).minImageExtent.width,  (*capabilities).maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, (*capabilities).minImageExtent.height, (*capabilities).maxImageExtent.height);

        return actualExtent;
    }
}

RenderInstance::RenderInstance(int width, int height)
    : m_FrameIndex(0)
{
    // GLFW Initialization
    // ----------------------
    
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
    
    m_GLFW = glfwCreateWindow(width, height, "High-Performance Hair Renderer", nullptr, nullptr);

    // Create Vulkan Instance
    // ----------------------

    VkApplicationInfo appInfo =
    {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Render Instance",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_3
    };

    // Sample GLFW for any extensions it needs. 
    uint32_t extensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

    std::vector<const char*> allExtensions;

    // Copy GLFW extensions. 
    for (uint32_t i = 0; i < extensionCount; ++i)
        allExtensions.push_back(glfwExtensions[i]);

#if __APPLE__
    // MoltenVK compatibility. 
    allExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    allExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    const std::vector<const char*> validationLayers = 
    {
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo instanceCreateInfo =
    {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledExtensionCount   = (uint32_t)allExtensions.size(),
        .ppEnabledExtensionNames = allExtensions.data(),

    #if 1
        .enabledLayerCount       = 1,
        .ppEnabledLayerNames     = validationLayers.data(),
    #else
        .enabledLayerCount       = 0,
        .ppEnabledLayerNames     = nullptr,
    #endif

    #if __APPLE__
        // For MoltenVK. 
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
    #endif
    };

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_VKInstance) != VK_SUCCESS) 
        throw std::runtime_error("failed to create instance!");

    
    // Create an OS-compatible Surface. 
    // ----------------------
    
#if __APPLE__
    {  
        // macOS-compatibility
        // ----------------------
        
        VkMacOSSurfaceCreateInfoMVK createInfo =
        {
            .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
            .pNext = nullptr,
            .flags = 0,
            .pView = GetMetalLayer(m_GLFW)
        };

        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
        vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(m_VKInstance, "vkCreateMacOSSurfaceMVK");

        if (!vkCreateMacOSSurfaceMVK) 
        {
            // Note: This call will fail unless we add VK_MVK_MACOS_SURFACE_EXTENSION_NAME extension. 
            throw std::runtime_error("Unabled to get pointer to function: vkCreateMacOSSurfaceMVK");
        }

        if (vkCreateMacOSSurfaceMVK(m_VKInstance, &createInfo, nullptr, &m_VKSurface)!= VK_SUCCESS) 
            throw std::runtime_error("failed to create surface.");
    }
#endif

    // Create Vulkan Physical Device
    // ----------------------

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_VKInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("no physical graphics devices found.");

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(m_VKInstance, &deviceCount, physicalDevices.data());

    // Lazily choose the first one. 
    m_VKPhysicalDevice = physicalDevices[0];

    // Queue Families
    // ----------------------

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_VKPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_VKPhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (int i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            m_QueueFamilyIndexGraphics = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_VKPhysicalDevice, i, m_VKSurface, &presentSupport);

        if (presentSupport)
            m_QueueFamilyIndexPresent = i;
    }

    // Create Vulkan Device
    // ----------------------
    
    if (m_QueueFamilyIndexGraphics == UINT_MAX || m_QueueFamilyIndexPresent == UINT_MAX)
        throw std::runtime_error("no graphics or present queue for the device.");

    if (m_QueueFamilyIndexGraphics != m_QueueFamilyIndexPresent)
        throw std::runtime_error("no support for different graphics and present queue.");
        
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo =
    {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_QueueFamilyIndexGraphics,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    // Specify the physical features to use. 
    VkPhysicalDeviceFeatures deviceFeatures = {};

#if __APPLE__
    const char* enabledExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, "VK_KHR_portability_subset" };
#else
    const char* enabledExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
#endif

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature =
    {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE
    };

    VkDeviceCreateInfo deviceCreateInfo = 
    {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dynamicRenderingFeature,
        .pQueueCreateInfos       = &queueCreateInfo,
        .queueCreateInfoCount    = 1,
        .pEnabledFeatures        = &deviceFeatures,
        .ppEnabledExtensionNames = enabledExtensions,
    #if __APPLE__
        .enabledExtensionCount   = 3,
    #else
        .enabledExtensionCount   = 2,
    #endif
        .enabledLayerCount       = 0
    };

    if (vkCreateDevice(m_VKPhysicalDevice, &deviceCreateInfo, nullptr, &m_VKDevice) != VK_SUCCESS) 
        throw std::runtime_error("failed to create logical device.");

    // Acquire queues
    vkGetDeviceQueue(m_VKDevice, m_QueueFamilyIndexGraphics, 0, &m_VKGraphicsQueue);
    vkGetDeviceQueue(m_VKDevice, m_QueueFamilyIndexPresent,  0, &m_VKPresentQueue);

    // Function pointers to the dynamic rendering extension API. 
	m_VKCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(m_VKDevice, "vkCmdBeginRenderingKHR"));
	m_VKCmdEndRenderingKHR   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>  (vkGetDeviceProcAddr(m_VKDevice, "vkCmdEndRenderingKHR"  ));

    // Create Swapchain
    // ----------------------

    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_VKPhysicalDevice, m_VKSurface);

    // Store the format and extent
    VkSurfaceFormatKHR format = ChooseSwapSurfaceFormat(swapChainSupport);
    VkExtent2D         extent = ChooseSwapExtent(&swapChainSupport.capabilities, NULL);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo =
    {
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface               = m_VKSurface,
        .minImageCount         = imageCount,
        .imageExtent           = extent,
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform          = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = ChooseSwapPresentMode(swapChainSupport),
        .clipped               = VK_TRUE,
        .oldSwapchain          = VK_NULL_HANDLE,
        .imageFormat           = format.format,
        .imageColorSpace       = format.colorSpace,
        .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = NULL
    };

    if(vkCreateSwapchainKHR(m_VKDevice, &createInfo, NULL, &m_VKSwapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain.");

    // Command Pool
    VkCommandPoolCreateInfo commandPoolInfo
    {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_QueueFamilyIndexGraphics
    };

    if (vkCreateCommandPool(m_VKDevice, &commandPoolInfo, nullptr, &m_VKCommandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool.");

    // Fetch image count.
    vkGetSwapchainImagesKHR(m_VKDevice, m_VKSwapchain, &m_VKSwapchainImageCount, nullptr);

    std::vector<VkImage> swapChainImages(m_VKSwapchainImageCount);
    vkGetSwapchainImagesKHR(m_VKDevice, m_VKSwapchain, &m_VKSwapchainImageCount, swapChainImages.data());

    // Create frames.
    m_Frames.resize(m_VKSwapchainImageCount);

    for (size_t i = 0; i < m_VKSwapchainImageCount; ++i)
    {
        // Swapchain image. 

        m_Frames[i].backBuffer = swapChainImages[i];

        // Swapchain image view.

        VkImageViewCreateInfo createInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image        = m_Frames[i].backBuffer,
            .viewType     = VK_IMAGE_VIEW_TYPE_2D,
            .format       = format.format,

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

        if (vkCreateImageView(m_VKDevice, &createInfo, NULL, &m_Frames[i].backBufferView) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain image view.");

        // Synchronization primitives.

        VkSemaphoreCreateInfo semaphoreInfo { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        
        VkFenceCreateInfo fenceInfo
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,

            // Create in a signaled state. 
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        if (vkCreateSemaphore(m_VKDevice, &semaphoreInfo, nullptr, &m_Frames[i].semaphoreWait)   != VK_SUCCESS ||
            vkCreateSemaphore(m_VKDevice, &semaphoreInfo, nullptr, &m_Frames[i].semaphoreSignal) != VK_SUCCESS ||
            vkCreateFence    (m_VKDevice, &fenceInfo,     nullptr, &m_Frames[i].fenceWait)       != VK_SUCCESS)
            throw std::runtime_error("failed to create swapchain synchronziation primitives.");

        // Command buffer.

        VkCommandBufferAllocateInfo commandAllocateInfo
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = m_VKCommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        if (vkAllocateCommandBuffers(m_VKDevice, &commandAllocateInfo, &m_Frames[i].commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffer.");
    }

    m_VKViewport =
    { 
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float) extent.width,
        .height   = (float) extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    m_VKScissor =
    {
        .offset.x = 0,
        .offset.y = 0,
        .extent   = extent
    };
}

RenderInstance::~RenderInstance()
{
    WaitForIdle();

    vkDestroyCommandPool(m_VKDevice, m_VKCommandPool, nullptr);

    for (const auto& frame : m_Frames)
    {
        vkDestroySemaphore(m_VKDevice, frame.semaphoreSignal, nullptr);
        vkDestroySemaphore(m_VKDevice, frame.semaphoreWait,   nullptr);
        vkDestroyFence    (m_VKDevice, frame.fenceWait,       nullptr);
        vkDestroyImageView(m_VKDevice, frame.backBufferView,  nullptr);
    }

    vkDestroySwapchainKHR(m_VKDevice, m_VKSwapchain, nullptr);
    vkDestroyDevice(m_VKDevice, nullptr);
    vkDestroySurfaceKHR(m_VKInstance, m_VKSurface, nullptr);
    vkDestroyInstance(m_VKInstance, nullptr);
    
    glfwDestroyWindow(m_GLFW);
    glfwTerminate();
}

int RenderInstance::Execute(std::function<void(RenderContext)> renderCallback)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo
    {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags            = 0,
        .pInheritanceInfo = nullptr
    };

    while(!glfwWindowShouldClose(m_GLFW)) 
    {
        glfwPollEvents();

        // Grab the current frame primtives.
        auto* frame = &m_Frames[m_FrameIndex];

        // Pause thread until the fence is signaled.
        vkWaitForFences(m_VKDevice, 1, &frame->fenceWait, VK_TRUE, UINT64_MAX);

        // Immediately reset fence once signaled. 
        vkResetFences(m_VKDevice, 1, &frame->fenceWait);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(m_VKDevice, m_VKSwapchain, UINT64_MAX, frame->semaphoreWait, VK_NULL_HANDLE, &imageIndex);

        // Reset the command buffer for this frame.
        vkResetCommandBuffer(frame->commandBuffer, 0x0);

        vkBeginCommandBuffer(frame->commandBuffer, &commandBufferBeginInfo);
        {
            // Back-buffer barrier.
            VkImageMemoryBarrier image_memory_barrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .image = frame->backBuffer,
                .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                }
            };

            vkCmdPipelineBarrier(
                frame->commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
                0,
                0,
                nullptr,
                0,
                nullptr,
                1, // imageMemoryBarrierCount
                &image_memory_barrier // pImageMemoryBarriers
            );

            // Invoke user render callback to write commands. 
            RenderContext context
            {
                .commandBuffer      = frame->commandBuffer,
                .backBufferView     = frame->backBufferView,
                .backBufferScissor  = m_VKScissor,
                .backBufferViewport = m_VKViewport,
                .renderBegin        = m_VKCmdBeginRenderingKHR,
                .renderEnd          = m_VKCmdEndRenderingKHR
            };

            renderCallback(context);

            // Back-buffer barrier

            VkImageMemoryBarrier2 imageMemoryBarrier =
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .oldLayout           = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = m_QueueFamilyIndexGraphics,
                .dstQueueFamilyIndex = m_QueueFamilyIndexPresent
            };

            VkDependencyInfoKHR dependencyInfo =
            {
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &imageMemoryBarrier
            };

            image_memory_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .image = frame->backBuffer,
                .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                }
            };

            vkCmdPipelineBarrier(
                frame->commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
                0,
                0,
                nullptr,
                0,
                nullptr,
                1, // imageMemoryBarrierCount
                &image_memory_barrier // pImageMemoryBarriers
            );
        }
        vkEndCommandBuffer(frame->commandBuffer);

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        // Submit

        VkSubmitInfo submitInfo 
        { 
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &frame->semaphoreWait,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &frame->semaphoreSignal,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &frame->commandBuffer
        };

        if (vkQueueSubmit(m_VKGraphicsQueue, 1, &submitInfo, frame->fenceWait) != VK_SUCCESS)
            throw std::runtime_error("failed to submit command buffer to graphics queue.");

        // Present 

        VkPresentInfoKHR presentInfo
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &frame->semaphoreSignal,
            .swapchainCount     = 1,
            .pSwapchains        = &m_VKSwapchain,
            .pImageIndices      = &imageIndex,
            .pResults           = nullptr
        };

        vkQueuePresentKHR(m_VKPresentQueue, &presentInfo);

        m_FrameIndex = (m_FrameIndex + 1) % m_VKSwapchainImageCount;
    }

    return 0;
}