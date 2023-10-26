Texture2D       staticMeshTexture   : register(t0, space0);
Texture2D       bloomTexture        : register(t1, space0);
Texture2D       uiTexture           : register(t2, space0);
SamplerState    blitSampler         : register(s3, space0);
float4 main(float2 texcoord         : TEXCOORD) : SV_TARGET
{
    float4 staticMeshColor = staticMeshTexture.Sample(blitSampler, texcoord);
    float4 bloomColor = bloomTexture.Sample(blitSampler, texcoord);
    if(dot(bloomColor.rgb, bloomColor.rgb) > 0)
        staticMeshColor.rgb += bloomColor.rgb * 0.04;
    float4 uiColor = uiTexture.Sample(blitSampler, texcoord);
    return lerp(staticMeshColor, uiColor, uiColor.a);
}