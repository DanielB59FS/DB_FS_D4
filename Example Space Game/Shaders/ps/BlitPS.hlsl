struct PS_INPUT
{
    float2 texCoord : TEXCOORD;
    float4 srcRect  : SRC_RECT;
};
Texture2D       blitTexture : register(t0, space0);
SamplerState    blitSampler : register(s1, space0);
float4 main(PS_INPUT input) : SV_TARGET
{
    float2 texCoord = input.srcRect.xy + (input.texCoord * input.srcRect.zw);
    return blitTexture.Sample(blitSampler, texCoord);
}