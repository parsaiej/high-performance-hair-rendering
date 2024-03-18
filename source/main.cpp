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

// Device Data
// ----------------------

struct Vertex
{
    glm::vec3 positionOS;
};

struct Mesh
{
    uint32_t indexCount;

    std::unique_ptr<Buffer> indexBuffer;
    std::unique_ptr<Buffer> vertexBuffer;
};

struct PerFrameData
{
    glm::mat4 matrixVP;
};

// Resources
// ----------------------

static std::unique_ptr<Pipeline>          s_TrianglePipeline;
static std::vector<std::unique_ptr<Mesh>> s_Meshes;
static std::unique_ptr<PerFrameData>      s_PerFrameData;

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

    s_PerFrameData = std::make_unique<PerFrameData>();
    {
        s_PerFrameData->matrixVP = matrixVP;
    }

    // Create mesh data

    s_Meshes.resize(meshList.size());

    for (int i = 0; i < meshList.size(); i++)
    {
        s_Meshes[i] = std::make_unique<Mesh>();

        // Load the usd mesh. 
        pxr::UsdGeomMesh sceneMesh(stage->GetPrimAtPath(meshList[i]));
        
        // Extracting vertex positions
        pxr::VtArray<pxr::GfVec3f> points;
        sceneMesh.GetPointsAttr().Get(&points);

        uint32_t vertexBufferSize = sizeof(points[0]) * points.size();

        // Allocate buffer to accomodate vertex positions. 
        s_Meshes[i]->vertexBuffer = std::make_unique<Buffer>(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s_Meshes[i]->vertexBuffer->SetData(points.data(), vertexBufferSize);

        // Extracting face vertex indices
        pxr::VtArray<int> indices;
        sceneMesh.GetFaceVertexIndicesAttr().Get(&indices);

        uint32_t indexBufferSize = sizeof(int) * indices.size();

        s_Meshes[i]->indexCount  = indices.size();
        s_Meshes[i]->indexBuffer = std::make_unique<Buffer>(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s_Meshes[i]->indexBuffer->SetData(indices.data(), indexBufferSize);
    }

    // Create device resources

    s_TrianglePipeline = std::make_unique<Pipeline>();
    {
        // Set push constants.

        VkPushConstantRange perFrameConstants
        {
            .offset     = 0,
            .size       = sizeof(PerFrameData),
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        };
        s_TrianglePipeline->SetPushConstants( { perFrameConstants } );

        // Vertex bindings.

        VkVertexInputBindingDescription vertexDataBinding
        {
            .binding   = 0,
            .stride    = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        s_TrianglePipeline->SetVertexInputBindings( { vertexDataBinding } );

        // Vertex attributes.

        VkVertexInputAttributeDescription attribute0
        {
            .binding  = 0,
            .location = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, positionOS)
        };

        s_TrianglePipeline->SetVertexInputAttributes( { attribute0 } );

        // Configure the triangle pipeline.
        s_TrianglePipeline->SetShaderProgram("assets/vert.spv", "assets/frag.spv");
        s_TrianglePipeline->SetScissor(context.backBufferScissor);
        s_TrianglePipeline->SetViewport(context.backBufferViewport);
        s_TrianglePipeline->SetColorTargetFormats( { context.backBufferFormat } );
        s_TrianglePipeline->Commit();
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
        s_TrianglePipeline->UpdateLayout(cmd, s_PerFrameData.get(), sizeof(PerFrameData));
        s_TrianglePipeline->Bind(cmd);
    }

    for (const auto& mesh : s_Meshes)
    {
        auto vertexBuffers = mesh->vertexBuffer->Get();

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, mesh->indexBuffer->Get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh->indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

void ReleaseFunc(ReleaseContext context)
{
    s_TrianglePipeline->Release(context.device);

    for (auto& mesh : s_Meshes)
    {
        mesh->vertexBuffer->Release(context.device);
        mesh->indexBuffer->Release(context.device);
    }
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

        return renderInstance.Execute(InitFunc, RenderFunc, ReleaseFunc);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1; 
    }
}