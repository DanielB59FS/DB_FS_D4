struct ROOT_CONSTANTS
{
    float4x4 viewProj;
};
#ifdef __spirv__
[[vk::push_constant]]
#endif
ROOT_CONSTANTS root_constants;
struct VS_INPUT
{
    float3 position         : POSITION;
    float4x4 world          : TRANSFORM;
};
float4 main(VS_INPUT input) : SV_Position
{
    float4 worldPosition = mul(float4(input.position, 1), input.world);
    return mul(worldPosition, root_constants.viewProj);
};