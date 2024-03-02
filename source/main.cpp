// Include
// ----------------------

#if __APPLE__
    #define VK_USE_PLATFORM_MACOS_MVK
#endif

#if __APPLE__
    #define GLFW_EXPOSE_NATIVE_COCOA
    
    // Objective-C wrapper to bind a KHR surface to native macOS window. 
    #include "../external/MetalUtility/MetalUtility.h"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <iostream>

// Command-line parser utility. 
#include <cxxopts.hpp>

// Small vk wrappers
#include "shader.h"
#include "swapchain.h"

#define APPLICATION_NAME "High-Performance Hair Rendering"

// Util
// ----------------------

struct QueueFamilyIndices
{
    uint32_t graphics;
    uint32_t present;
};

QueueFamilyIndices GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;
    { 
        indices.graphics = UINT_MAX;
        indices.present  = UINT_MAX;
    }

    for (int i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
            indices.graphics = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if (presentSupport)
            indices.present = i;
    }

    return indices;
}

// Implementation
// ----------------------

struct Params
{
    int width;
    int height;
};

void Execute(Params params)
{
    // GLFW Initialization
    // ----------------------

    GLFWwindow* window;
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
        
        window = glfwCreateWindow(params.width, params.height, "High-Performance Hair Renderer", nullptr, nullptr);
    }

    // Create Vulkan Instance
    // ----------------------

    VkInstance instance;
    {
        VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = APPLICATION_NAME,
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

        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &appInfo,
            .enabledExtensionCount   = (uint32_t)allExtensions.size(),
            .ppEnabledExtensionNames = allExtensions.data(),

        #ifdef DEBUG
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

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) 
            throw std::runtime_error("failed to create instance!");
    }

    // Create an OS-compatible Surface. 
    // ----------------------
    VkSurfaceKHR surface;

#if __APPLE__
    {  
        // macOS-compatibility
        // ----------------------
        
        VkMacOSSurfaceCreateInfoMVK createInfo =
        {
            .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
            .pNext = nullptr,
            .flags = 0,
            .pView = GetMetalLayer(window)
        };

        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
        vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");

        if (!vkCreateMacOSSurfaceMVK) 
        {
            // Note: This call will fail unless we add VK_MVK_MACOS_SURFACE_EXTENSION_NAME extension. 
            throw std::runtime_error("Unabled to get pointer to function: vkCreateMacOSSurfaceMVK");
        }

        if (vkCreateMacOSSurfaceMVK(instance, &createInfo, nullptr, &surface)!= VK_SUCCESS) 
            throw std::runtime_error("failed to create surface.");
    }
#endif

    // Create Vulkan Physical Device
    // ----------------------

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("no physical graphics devices found.");

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        // Lazily choose the first one. 
        physicalDevice = physicalDevices[0];
    }

    // Query Device Queues
    // ----------------------

    QueueFamilyIndices queueIndices = GetQueueFamilyIndices(physicalDevice, surface);

    // Create Vulkan Device
    // ----------------------
    
    VkDevice device = VK_NULL_HANDLE;
    {
        if (queueIndices.graphics == UINT_MAX || queueIndices.present == UINT_MAX)
            throw std::runtime_error("no graphics or present queue for the device.");

        if (queueIndices.graphics != queueIndices.present)
            throw std::runtime_error("no support for different graphics and present queue.");
            
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueCreateInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueIndices.graphics,
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

        VkDeviceCreateInfo createInfo = 
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

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) 
            throw std::runtime_error("failed to create logical device.");
    }

    // Function pointers to the dynamic rendering extension API. 
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR  = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR"));
	PFN_vkCmdEndRenderingKHR   vkCmdEndRenderingKHR    = reinterpret_cast<PFN_vkCmdEndRenderingKHR>  (vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR"  ));

    // Create Swap Chain
    // ----------------------

    SwapChainParams swapChainParams =
    {
        .physicalDevice = physicalDevice,
        .device         = &device,
        .surface        = surface,
        .window         = window,

        .queueIndexGraphics = queueIndices.graphics,
        .queueIndexPresent  = queueIndices.present,
    };

    SwapChain swapChain;
    TryCreateSwapChain(swapChainParams, &swapChain);

    // Create Graphics Pipeline
    // ----------------------

    Shader shaderVert;
    Shader shaderFrag;
    
    if (!CreateShader(device, VK_SHADER_STAGE_VERTEX_BIT,   &shaderVert, "build/vert.spv") ||
        !CreateShader(device, VK_SHADER_STAGE_FRAGMENT_BIT, &shaderFrag, "build/frag.spv"))
        throw std::runtime_error("failed to create shaders.");

    // Vertex Layout

    VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr
    };

    // Input Assembly

    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport / Scissor

    VkViewport viewport =
    { 
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float) swapChain.extent.width,
        .height   = (float) swapChain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor =
    {
        .offset.x = 0,
        .offset.y = 0,
        .extent   = swapChain.extent
    };

    VkPipelineViewportStateCreateInfo viewportState =
    {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor
    };

    // Rasterizer

    VkPipelineRasterizationStateCreateInfo rasterizer =
    {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f, 
        .depthBiasSlopeFactor    = 0.0f 
    };

    // MSAA

    VkPipelineMultisampleStateCreateInfo multisampling =
    {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = VK_FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f,
        .pSampleMask           = nullptr, 
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,   
    };

    // Blend State

    VkPipelineColorBlendAttachmentState colorBlendAttachment =
    {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable    = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo colorBlending =
    {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment
    };

    // Layout

    VkPipelineLayoutCreateInfo pipelineLayoutInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout.");

    VkPipelineShaderStageCreateInfo shaderStages[2] = 
    {
        shaderVert.stageInfo,
        shaderFrag.stageInfo
    };

    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = 
    {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapChain.format.format
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = 
    {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &pipeline_rendering_create_info,
        .stageCount          = 2,
        .pStages             = shaderStages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = nullptr,
        .layout              = pipelineLayout,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,

        // None due to dynamic rendering extension. 
        .renderPass          = nullptr,
        .subpass             = 0
    };

    VkPipeline graphicsPipeline;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) 
        throw std::runtime_error("failed to create graphics pipeline.");

    // Command Pool
    // ----------------------

    VkCommandPoolCreateInfo commandPoolInfo
    {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueIndices.graphics
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool.");

    // Commands
    // ----------------------

    VkCommandBufferAllocateInfo commandAllocateInfo
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &commandAllocateInfo, &commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffer.");

    // Command Recording

    VkCommandBufferBeginInfo commandBufferBeginInfo
    {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags            = 0,
        .pInheritanceInfo = nullptr
    };
    
    // Fetch Queues
    // ----------------------

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueIndices.graphics, 0, &graphicsQueue);

    VkQueue presentQueue;
    vkGetDeviceQueue(device, queueIndices.present, 0, &presentQueue);

    // Render-loop
    // ----------------------
    
    while(!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        vkWaitForFences(device, 1, &swapChain.inFlightFence, VK_TRUE, UINT64_MAX);

        vkResetFences(device, 1, &swapChain.inFlightFence);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain.primitive, UINT64_MAX, swapChain.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffer, 0);

        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
        {
            VkClearValue clearValue { .color = { 1, 0, 0, 0} }; 

            VkRenderingAttachmentInfoKHR colorAttachment
            {
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .imageView   = swapChain.imageViews[imageIndex],
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = clearValue
            };

            VkRenderingInfoKHR renderInfo
            {
                .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                .renderArea           = scissor,
                .layerCount           = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachment,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRenderingKHR(commandBuffer, &renderInfo);

            // begin

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            // end

            vkCmdEndRenderingKHR(commandBuffer);
        }
        vkEndCommandBuffer(commandBuffer);

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        // Submit

        VkSubmitInfo submitInfo 
        { 
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &swapChain.imageAvailableSemaphore,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &swapChain.renderFinishedSemaphore,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &commandBuffer
        };

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, swapChain.inFlightFence) != VK_SUCCESS)
            throw std::runtime_error("failed to submit command buffer to graphics queue.");

        // Present 

        VkPresentInfoKHR presentInfo
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &swapChain.renderFinishedSemaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &swapChain.primitive,
            .pImageIndices      = &imageIndex,
            .pResults           = nullptr
        };

        vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    // Vulkan Destroy
    // ----------------------

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    ReleaseShader(device, &shaderVert);
    ReleaseShader(device, &shaderFrag);

    ReleaseSwapChain(device, &swapChain);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    // GLFW Destroy
    // ----------------------
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return;
}


// Entry
// ----------------------

int main(int argc, char *argv[]) 
{
    cxxopts::Options options(APPLICATION_NAME, "A brief description");

    options.add_options()
        ("width",  "Viewport Width",  cxxopts::value<int>()->default_value("800"))
        ("height", "Viewport Height", cxxopts::value<int>()->default_value("600"))
        ("help",   "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    try 
    {
        Params params 
        {
            .width  = result["width"] .as<int>(),
            .height = result["height"].as<int>(),
        };

        Execute(params); 
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0; 
}