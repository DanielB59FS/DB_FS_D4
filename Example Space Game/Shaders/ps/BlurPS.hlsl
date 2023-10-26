struct ROOT_CONSTANTS
{
    uint blurDirection;
};
#ifdef __spirv__
[[vk::push_constant]]
#endif
ROOT_CONSTANTS root_constants;
struct PS_INPUT
{
    float2 texCoord : TEXCOORD;
};
Texture2D       inputTexture : register(t0, space0);
SamplerState    inputSampler : register(s1, space0);
float4 main(PS_INPUT input) : SV_TARGET
{
    float weight[5];
    weight[0] = 0.227027;
    weight[1] = 0.1945946;
    weight[2] = 0.1216216;
    weight[3] = 0.054054;
    weight[4] = 0.016216;
    float2 textureSize;
    inputTexture.GetDimensions(textureSize.x, textureSize.y);
    float2 tex_offset = 1.0 / textureSize * 2;
    float3 outColor = inputTexture.Sample(inputSampler, input.texCoord).rgb;
    for(int i = 0; i < 5; ++i)
    {
        if(root_constants.blurDirection == 0)
        {
            outColor += inputTexture.Sample(inputSampler, input.texCoord + float2(tex_offset.x * i, 0.0)).rgb * weight[i];
            outColor += inputTexture.Sample(inputSampler, input.texCoord - float2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
        else
        {
            outColor += inputTexture.Sample(inputSampler, input.texCoord + float2(0.0, tex_offset.y * i)).rgb * weight[i];
            outColor += inputTexture.Sample(inputSampler, input.texCoord - float2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    return float4(outColor, 1.0);
}