struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.texcoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4((output.texcoord * 2.f) - 1.f, 0.f, 1.f);
    return output;
}