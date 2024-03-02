#ifndef SHADER
#define SHADER

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct Shader
{
    VkShaderModule                  module;
    VkPipelineShaderStageCreateInfo stageInfo;
    char*                           byteCode;
    long                            byteCodeSize;
};

bool CreateShader(VkDevice device, VkShaderStageFlagBits stage, Shader* shader, const char* filePath);

void ReleaseShader(VkDevice device, Shader* shader);

#endif//SHADER