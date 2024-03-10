#include <iostream>

#include <VK-Demo-Wrappers/RenderInstance.h>

// Implementation
// ----------------------

void HairRenderingCommands(Wrappers::RenderContext context)
{
    VkRenderingAttachmentInfoKHR colorAttachment
    {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView   = context.backBufferView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue.color = { 0, 1, 0, 1 }
    };

    VkRenderingInfoKHR renderInfo
    {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .renderArea           = context.backBufferScissor,
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr
    };

    context.renderBegin(context.commandBuffer, &renderInfo);

    //vkCmdSetViewport(context.commandBuffer, 0, 1, &context.backBufferViewport);
    //vkCmdSetScissor (context.commandBuffer, 0, 1, &context.backBufferScissor);
    //vkCmdDraw(context.commandBuffer, 3, 1, 0, 0);

    context.renderEnd(context.commandBuffer);
}

// Entry
// ----------------------

int main(int argc, char *argv[]) 
{
    try 
    {
        Wrappers::RenderInstance renderInstance(800, 600);

        return renderInstance.Execute(HairRenderingCommands);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1; 
    }

    return 0; 
}