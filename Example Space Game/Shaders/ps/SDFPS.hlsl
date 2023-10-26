struct ROOT_CONSTANTS
{
    float4 fontColor;
    float4 outlineColor;
    float outlineWidth;
};
#ifdef __spirv__
[[vk::push_constant]]
#endif
ROOT_CONSTANTS root_constants;
struct PS_INPUT
{
    float2 texcoord : TEXCOORD;
};
Texture2D       fontAtlasTexture    : register(t0, space0);
SamplerState    fontAtlasSampler    : register(s1, space0);
float4 main(PS_INPUT input) : SV_TARGET
{
    float distance = fontAtlasTexture.Sample(fontAtlasSampler, input.texcoord).a;
    float smoothWidth = fwidth(distance);
    float alpha = smoothstep(0.5 - smoothWidth, 0.5 + smoothWidth, distance);
    float border = smoothstep(root_constants.outlineWidth - smoothWidth, root_constants.outlineWidth + smoothWidth, distance);
    float3 outColor = lerp(root_constants.outlineColor.xyz, root_constants.fontColor.xyz, border);
    return float4(outColor, alpha);
}