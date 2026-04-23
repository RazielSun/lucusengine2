#ifndef LIGHT_SLANG
#define LIGHT_SLANG

[[vk::binding(0, 4)]]
Texture2D gShadowMap : register(t1);
[[vk::binding(0, 6)]]
SamplerState gShadowMapSampler : register(s1);

float computeShadow(float4 lightClipPos, float lambert)
{
    float3 shadowCoord = lightClipPos.xyz / lightClipPos.w;
    float2 shadowUV = float2(shadowCoord.x * 0.5f + 0.5f, 0.5f - shadowCoord.y * 0.5f);

    bool insideShadowMap =
        shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
        shadowCoord.z >= 0.0f && shadowCoord.z <= 1.0f;

    float shadowDepth = gShadowMap.Sample(gShadowMapSampler, shadowUV).r;
    float currentDepth = shadowCoord.z;
    float bias = max(0.0005f, 0.0025f * (1.0f - lambert));
    float lit = insideShadowMap && currentDepth - bias > shadowDepth ? 0.25f : 1.0f;
    return lit;
}

#endif /* LIGHT_SLANG */