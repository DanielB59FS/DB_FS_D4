TextureCube     cubeMapTexture  : register(t0, space0);
SamplerState    cubeMapSampler  : register(s1, space0);
float4 main(float3 texcoord     : TEXCOORD) : SV_TARGET
{
    return cubeMapTexture.Sample(cubeMapSampler, texcoord);
}