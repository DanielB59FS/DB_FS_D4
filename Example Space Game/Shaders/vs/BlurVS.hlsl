struct VS_INPUT
{
    uint vertexID   : SV_VertexID;
};
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.texCoord = float2((input.vertexID << 1) & 2, input.vertexID & 2);
    output.position = float4((output.texCoord * 2.0f) - 1.0f, 0, 1);
    return output;
}