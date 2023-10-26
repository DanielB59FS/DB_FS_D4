struct VS_INPUT
{
    float2 position : POSITION;
    float3 texcoord : TEXCOORD;
};
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.position = float4(input.position, 0.0, 1.0);
    output.texcoord = input.texcoord.xy;
    return output;
}