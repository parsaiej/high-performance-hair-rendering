static float2 positions[3] = 
{
    float2(+0.0, -0.5),
    float2(+0.5, +0.5),
    float2(-0.5, +0.5)
};

static float3 colors[3] = 
{
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

struct PerFrameData
{
    float4x4 matrixVP;
};

[[vk::push_constant]]
PerFrameData perFrameData;

struct VertexInput
{
    uint vertexID : SV_VertexID;
};

struct Interpolators
{
    float4 positionCS : SV_POSITION;

    [[vk::location(0)]]
    float3 color : TEXCOORD0;
};

Interpolators main(VertexInput input)
{
    Interpolators o = (Interpolators)0;
    {
        float4 positionOS = float4(positions[input.vertexID], 0, 1);

        o.positionCS = mul(positionOS, perFrameData.matrixVP);
        o.color      = colors[input.vertexID];
    }

    return o;
}