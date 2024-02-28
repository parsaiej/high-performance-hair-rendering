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

#include <vulkan/utility/vk_format_utils.h>

#include <stdio.h>

#include "shader.h"
#include "swapchain.h"

// Util
// ----------------------

#define MAX_INSTANCE_EXTENSION 16
#define MAX_PHYSICAL_DEVICES    1
#define MAX_QUEUE_FAMILIES     16

typedef struct {

    uint32_t graphics;
    uint32_t present;

} QueueFamilyIndices;

QueueFamilyIndices GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties queueFamilies[MAX_QUEUE_FAMILIES];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

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

// Entry
// ----------------------

int main(int argc, char *argv[]) 
{
    // GLFW Initialization
    // ----------------------

    GLFWwindow* window;
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
        
        window = glfwCreateWindow(800, 600, "High-Performance Hair Renderer", NULL, NULL);
    }

    // Create Vulkan Instance
    // ----------------------

    VkInstance instance;
    {
        VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = "High-Performance Hair Rendering",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3
        };

        // Sample GLFW for any extensions it needs. 
        uint32_t extensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

        const char* allExtensions[MAX_INSTANCE_EXTENSION];

        // Copy GLFW extensions. 
        for (uint32_t i = 0; i < extensionCount; ++i)
            allExtensions[i] = glfwExtensions[i];

    #if __APPLE__
        // MoltenVK compatibility. 
        allExtensions[extensionCount++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        allExtensions[extensionCount++] = VK_MVK_MACOS_SURFACE_EXTENSION_NAME;
    #endif

        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &appInfo,
            .enabledExtensionCount   = extensionCount,
            .ppEnabledExtensionNames = allExtensions,
            .enabledLayerCount       = 0,
        #if __APPLE__
            // For MoltenVK. 
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        #endif
        };

        if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) 
        {
            printf("failed to create instance!");
            return -1; 
        }
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
            .pNext = NULL,
            .flags = 0,
            .pView = GetMetalLayer(window)
        };

        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
        vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");

        if (!vkCreateMacOSSurfaceMVK) 
        {
            // Note: This call will fail unless we add VK_MVK_MACOS_SURFACE_EXTENSION_NAME extension. 
            printf("Unabled to get pointer to function: vkCreateMacOSSurfaceMVK");
            return -1;
        }

        if (vkCreateMacOSSurfaceMVK(instance, &createInfo, NULL, &surface)!= VK_SUCCESS) 
        {
            printf("failed to create surface!");
            return -1;
        }
    }
#endif

    // Create Vulkan Physical Device
    // ----------------------

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

        if (deviceCount == 0)
        {
            printf("no graphics device found.");
            return -1; 
        }

        VkPhysicalDevice physicalDevices[MAX_PHYSICAL_DEVICES];
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);

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
        {
            printf("no graphics or present queue for the device.");
            return -1; 
        }

        if (queueIndices.graphics != queueIndices.present)
        {
            printf("no support for different graphics and present queue.");
            return -1; 
        }
            
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

        if (vkCreateDevice(physicalDevice, &createInfo, NULL, &device) != VK_SUCCESS) 
        {
            printf("failed to create logical device!");
            return -1;
        }
    }

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

    if (CreateSwapChain(swapChainParams, &swapChain) != VK_SUCCESS)
    {
        printf("failed to create a swap chain.");
        return -1;
    }

    // Create Graphics Pipeline
    // ----------------------

    Shader shaderVert;
    Shader shaderFrag;
    
    if (!CreateShader(device, VK_SHADER_STAGE_VERTEX_BIT,   &shaderVert, "build/vert.spv") ||
        !CreateShader(device, VK_SHADER_STAGE_FRAGMENT_BIT, &shaderFrag, "build/frag.spv"))
        return -1;

    // Vertex Layout

    VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = NULL
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
        .pSampleMask           = NULL, 
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
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) != VK_SUCCESS)
    {
        printf("failed to create pipeline layout.");
        return -1; 
    }

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
        .pDepthStencilState  = NULL,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = NULL,
        .layout              = pipelineLayout,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,

        // None due to dynamic rendering extension. 
        .renderPass          = NULL,
        .subpass             = 0
    };

    VkPipeline graphicsPipeline;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) != VK_SUCCESS) 
    {
        printf("failed to create graphics pipeline.");
        return -1;
    }
    
    // Fetch Queues
    // ----------------------

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueIndices.graphics, 0, &graphicsQueue);

    VkQueue presentQueue;
    vkGetDeviceQueue(device, queueIndices.present, 0, &presentQueue);
    
    while(!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();
    }

    // Vulkan Destroy
    // ----------------------

    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);

    ReleaseShader(device, &shaderVert);
    ReleaseShader(device, &shaderFrag);

    ReleaseSwapChain(device, &swapChain);

    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    // GLFW Destroy
    // ----------------------
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return 0;
}
