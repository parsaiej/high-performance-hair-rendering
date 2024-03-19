
struct Interpolators
{
    float4 positionCS : SV_POSITION;

    [[vk::location(0)]]
    float3 color : TEXCOORD0;
};

float4 main(Interpolators input) : SV_TARGET0
{
    return float4(input.color, 1.0);
}