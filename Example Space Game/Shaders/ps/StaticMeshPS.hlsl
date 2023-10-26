struct PS_INPUT
{
    float4  normal          : NORMAL;
    float2  texcoord        : TEXCOORD0;
    float4  shadowCoord     : TEXCOORD1;
    float4  bloomColor      : BLOOM;
    uint    isGameObject    : GAMEOBJECT;
};
struct PS_OUTPUT
{
    float4 mainColor        : SV_TARGET0;
    float4 bloomColor       : SV_TARGET1;
};
Texture2D       baseColorTexture    : register(t0, space0);
SamplerState    baseColorSampler    : register(s1, space0);
Texture2D       shadowMapTexture    : register(t0, space1);
SamplerState    shadowMapSampler    : register(s1, space1);
PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output = (PS_OUTPUT)0;
    float ambientFactor = input.isGameObject ? 1 : 0.2;
    float lightFactor = max(saturate(dot(input.normal, normalize(float4(0, 1, -1, 0)))), ambientFactor);
    float4 baseColor = baseColorTexture.Sample(baseColorSampler, input.texcoord);
    input.shadowCoord /= input.shadowCoord.w;
    float2 samplerCoord = (input.shadowCoord.xy * 0.5) + 0.5;
    float dist = shadowMapTexture.Sample(shadowMapSampler, samplerCoord).r;
    float shadow = 1.0f;
    if(dist < input.shadowCoord.z && dot(input.bloomColor.rgb, input.bloomColor.rgb) == 0)
    {
        shadow = 0.2;
    }
    output.mainColor =  float4(baseColor.xyz * float3(0.75, 0.87, 0.92) * lightFactor * shadow, 1);
    output.mainColor += input.bloomColor;
    output.bloomColor = input.bloomColor;
    return output;
}