struct ROOT_CONSTANTS
{
    float4x4 skyBoxRotMatrix;
};
#ifdef __spirv__
[[vk::push_constant]]
#endif
ROOT_CONSTANTS root_constants;
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD;
};
static float3 skyboxVertices[] = {
    float3(-1.0f,  1.0f, -1.0f),
    float3(-1.0f, -1.0f, -1.0f),
    float3( 1.0f, -1.0f, -1.0f),
    float3( 1.0f, -1.0f, -1.0f),
    float3( 1.0f,  1.0f, -1.0f),
    float3(-1.0f,  1.0f, -1.0f),

    float3(-1.0f, -1.0f,  1.0f),
    float3(-1.0f, -1.0f, -1.0f),
    float3(-1.0f,  1.0f, -1.0f),
    float3(-1.0f,  1.0f, -1.0f),
    float3(-1.0f,  1.0f,  1.0f),
    float3(-1.0f, -1.0f,  1.0f),

    float3( 1.0f, -1.0f, -1.0f),
    float3( 1.0f, -1.0f,  1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3( 1.0f,  1.0f, -1.0f),
    float3( 1.0f, -1.0f, -1.0f),

    float3(-1.0f, -1.0f,  1.0f),
    float3(-1.0f,  1.0f,  1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3( 1.0f, -1.0f,  1.0f),
    float3(-1.0f, -1.0f,  1.0f),

    float3(-1.0f,  1.0f, -1.0f),
    float3( 1.0f,  1.0f, -1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3( 1.0f,  1.0f,  1.0f),
    float3(-1.0f,  1.0f,  1.0f),
    float3(-1.0f,  1.0f, -1.0f),

    float3(-1.0f, -1.0f, -1.0f),
    float3(-1.0f, -1.0f,  1.0f),
    float3( 1.0f, -1.0f, -1.0f),
    float3( 1.0f, -1.0f, -1.0f),
    float3(-1.0f, -1.0f,  1.0f),
    float3( 1.0f, -1.0f,  1.0f)
};
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.texcoord = skyboxVertices[vertexID];
    output.position = mul(float4(output.texcoord, 1), root_constants.skyBoxRotMatrix).xyww;
    output.texcoord.xy *= -1;
    return output;
}