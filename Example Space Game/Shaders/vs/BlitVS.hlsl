struct VS_INPUT
{
    uint vertexID   : SV_VertexID;
    float4 srcRect  : SRC_RECT;
    float4 dstRect  : DST_RECT;
};
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 srcRect  : SRC_RECT;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    uint vertexID = (input.vertexID / 3) + (input.vertexID % 3);
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(input.dstRect.xy + (output.texCoord * input.dstRect.zw), 0, 1);
    output.texCoord = output.texCoord * 0.5f;
    output.srcRect = input.srcRect;
    return output;
}