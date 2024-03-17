#include <iostream>
#include <memory>

#include <RenderInstance.h>
#include <GraphicsResource.h>

using namespace Wrappers;

#ifdef __APPLE__
    // MacOS fix for bug inside USD. 
    #define unary_function __unary_function
#endif

// USD Include
// ----------------------

static const char* s_USDPath;

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/usd/usdLux/distantLight.h>

// GLM Include
// ----------------------

#include <glm/glm.hpp>
// Include all GLM extensions
#include <glm/ext.hpp> // perspective, translate, rotate

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cxxopts.hpp>

// Resources
// ----------------------

#define PIPELINE_TRIANGLE "triangle"

// Push Constants
// ----------------------

struct PerFrameData
{
    glm::mat4 matrixVP;
};

static PerFrameData* s_PerFrameData;

// Implementation
// ----------------------

void InitFunc(InitializeContext context)
{
    // Parse USD Stage

    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(s_USDPath);

    if (!stage)
        throw std::runtime_error("failed to open USD stage.");

    // Scan over the stage and read in curves, surfaces, lights, cameras. 

    std::vector<pxr::SdfPath> curveList;
    std::vector<pxr::SdfPath> meshList;
    std::vector<pxr::SdfPath> cameraList;
    std::vector<pxr::SdfPath> sunList;
    {
        pxr::UsdPrimRange range = stage->Traverse();

        for (auto it = range.begin(); it != range.end(); ++it) 
        {
            const auto path = it->GetPath();

            if (it->IsA<pxr::UsdGeomCurves>()) 
                curveList.push_back(path);
            else if (it->IsA<pxr::UsdGeomMesh>()) 
                meshList.push_back(path);
            else if (it->IsA<pxr::UsdGeomCamera>()) 
                cameraList.push_back(path);
            else if (it->IsA<pxr::UsdLuxDistantLight>()) 
                sunList.push_back(path);
        }
    }

    // Check scene validity.

    if (cameraList.size() != 1 || curveList.size() == 0)
        throw std::runtime_error("the USD stage is invalid.");

    // Configure camera matrices. 

    glm::mat4 matrixVP;
    {
        // Fetch the first and only camera.
        pxr::UsdGeomCamera camera(stage->GetPrimAtPath(cameraList[0]));

        // Fetch underlying camera data object (at t=0). 
        auto frustum = camera.GetCamera(0).GetFrustum();

        // TODO: Might be able to remove glm from the project completely and just use the USD native matrix type.
        glm::mat4 view = glm::transpose(glm::make_mat4(frustum.ComputeViewMatrix().data()));
        glm::mat4 proj = glm::transpose(glm::make_mat4(frustum.ComputeProjectionMatrix().data()));

        // Compose the matrix.
        matrixVP = view * proj;
    }

    // Update the constants

    s_PerFrameData = (PerFrameData*)malloc(sizeof(PerFrameData));
    {
        s_PerFrameData->matrixVP = matrixVP;
    }

    // Create device resources

    context.resources[PIPELINE_TRIANGLE] = std::make_unique<Pipeline>();
    {
        auto trianglePipeline = static_cast<Pipeline*>(context.resources[PIPELINE_TRIANGLE].get());
        
        // Set push constants.
        VkPushConstantRange perFrameConstants
        {
            .offset     = 0,
            .size       = sizeof(PerFrameData),
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        };
        trianglePipeline->SetPushConstants( { perFrameConstants } );

        // Configure the triangle pipeline.
        trianglePipeline->SetShaderProgram("assets/vert.spv", "assets/frag.spv");
        trianglePipeline->SetScissor(context.backBufferScissor);
        trianglePipeline->SetViewport(context.backBufferViewport);
        trianglePipeline->SetColorTargetFormats( { context.backBufferFormat } );
        trianglePipeline->Commit();
    }
}

void RenderFunc(RenderContext context)
{
    // Unfortunately need to do this.
    auto vkCmdBeginRendering = context.renderBegin;
    auto vkCmdEndRendering   = context.renderEnd;

    // Shorthand for command.
    auto cmd = context.commandBuffer;

    VkRenderingAttachmentInfoKHR colorAttachment
    {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView   = context.backBufferView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue.color = { 0, 0, 0, 1 }
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

    vkCmdBeginRendering(cmd, &renderInfo);
    
    // Bind triangle pipeline
    {
        const auto trianglePipeline = static_cast<Pipeline*>(context.resources[PIPELINE_TRIANGLE].get());
        
        trianglePipeline->UpdateLayout(cmd, s_PerFrameData, sizeof(PerFrameData));
        trianglePipeline->Bind(cmd);
    }

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

// Entry
// ----------------------

int main(int argc, char *argv[]) 
{
    // Parse arguments
    
    cxxopts::Options options("USD Read Test", "Simple application to parse a USD.");

    options.add_options() ("path", "USD File Path", cxxopts::value<std::string>());

    auto result = options.parse(argc, argv);

    if (result.count("path") == 0)
        return -1; 

    s_USDPath = result["path"].as<std::string>().c_str();

    // Invoke implementation
    try 
    {
        RenderInstance renderInstance(600 * (16.0 / 9.0), 600);

        return renderInstance.Execute(InitFunc, RenderFunc);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1; 
    }
}