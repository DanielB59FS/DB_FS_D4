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
    float3 position : POSITION;
    float4x4 world  : TRANSFORM;
};
float4 main(VS_INPUT input) : SV_POSITION
{
    float4 outPosition = mul(mul(float4(input.position, 1.0), input.world), root_constants.viewProj);
    return float4(outPosition.x, outPosition.y, 0, outPosition.w);
}