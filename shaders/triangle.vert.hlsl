struct PerFrameData
{
    float4x4 matrixVP;
};

[[vk::push_constant]]
PerFrameData perFrameData;

struct VertexInput
{
    uint vertexID : SV_VertexID;

    [[vk::location(0)]]
    float3 positionOS : POSITION;

    [[vk::location(1)]]
    float3 normalOS : NORMAL;
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
        float4 positionOS = float4(input.positionOS, 1);

        o.positionCS = mul(positionOS, perFrameData.matrixVP);
        o.color      = input.normalOS;
    }

    return o;
}