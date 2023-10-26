struct ROOT_CONSTANTS
{
    float4x4 viewProj;
    float4x4 light;
};
#ifdef __spirv__
[[vk::push_constant]]
#endif
ROOT_CONSTANTS root_constants;
struct VS_INPUT
{
    float3      position        : POSITION;
    float3      texcoord        : TEXCOORD0;
    float3      normal          : NORMAL;
    float4x4    world           : TRANSFORM;
    float4      bloomColor      : BLOOM;
    uint        isGameObject    : GAMEOBJECT;
};
struct VS_OUTPUT
{
    float4      position        : SV_POSITION;
    float4      normal          : NORMAL;
    float2      texcoord        : TEXCOORD0;
    float4      shadowCoord     : TEXCOORD1;
    float4      bloomColor      : BLOOM;
    uint        isGameObject    : GAMEOBJECT;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    float4 worldPosition = mul(float4(input.position, 1.0), input.world);
    output.position = mul(worldPosition, root_constants.viewProj);
    float4 meshNormal = normalize(float4(input.normal, 0));
    output.normal = normalize(mul(meshNormal, input.world));
    output.texcoord = input.texcoord.xy;
    output.shadowCoord = mul(worldPosition, root_constants.light);
    output.isGameObject = input.isGameObject;
    output.bloomColor = input.bloomColor;
    return output;
}