#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

bool CreateShader(VkDevice device, VkShaderStageFlagBits stage, Shader* shader, const char* filePath)
{
    // Read byte code from file.
    // -------------------

    FILE* file = fopen(filePath, "rb");

    if (!file)
    {
        printf("failed to open bytecode file.");
        return false;
    }

    // Determine the byte code file size.

    fseek(file, 0, SEEK_END);
    shader->byteCodeSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate the byteCode memory. 
    shader->byteCode = (char*)malloc(shader->byteCodeSize + 1);

    if (!shader->byteCode )
    {
        printf("failed to allocate memory for shader bytecode.");
        return false;
    }

    // Read in the file to memory.
    size_t result = fread(shader->byteCode, 1, shader->byteCodeSize, file);

    if (result != shader->byteCodeSize)
    {
        printf("failed to read bytecode.");
        free(shader->byteCode);
        fclose(file);
        return false;
    }

    // Close file. 
    fclose(file);

    VkShaderModuleCreateInfo moduleInfo =
    {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader->byteCodeSize,
        .pCode    = (uint32_t*)shader->byteCode,
    };

    if (vkCreateShaderModule(device, &moduleInfo, NULL, &shader->module) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo* stageInfo = &shader->stageInfo;
    {
        stageInfo->sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo->pNext  = NULL;
        stageInfo->stage  = stage;
        stageInfo->module = shader->module;
        stageInfo->pName  = "main";
        stageInfo->flags  = 0;
        stageInfo->pSpecializationInfo = NULL;
    }

    return true;
}

void ReleaseShader(VkDevice device, Shader* shader)
{
    vkDestroyShaderModule(device, shader->module, NULL);
    free(shader->byteCode);
}