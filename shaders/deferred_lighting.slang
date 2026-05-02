#include "common/defines.slang"
#include "common/structs.slang"
#include "common/light.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 3)]]
ConstantBuffer<LightUniforms> gLight : register(b3);

[shader("vertex")]
VSOutput vsMain(uint vertexIndex : SV_VertexID)
{
    float2 positions[3] =
    {
        // Keep this order front-facing with back-face culling enabled.
        float2(-1.0,  3.0),
        float2(-1.0, -1.0),
        float2( 3.0, -1.0)
    };

    VSOutput o;
    o.position = float4(positions[vertexIndex], 0.0, 1.0);
#if TARGET_METAL || TARGET_DX12
    o.position.y *= -1.0; // Flip Y for Metal's coordinate system
#endif
    return o;
}

[[vk::binding(0, 10)]]
Texture2D gBufferA : register(t3);
[[vk::binding(0, 11)]]
Texture2D gBufferB : register(t4);
[[vk::binding(0, 12)]]
Texture2D gBufferC : register(t5);
[[vk::binding(0, 13)]]
Texture2D gBufferDepth : register(t6);
[[vk::binding(0, 14)]]
SamplerState gSampler : register(s2);

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target
{
    float2 screenSize = gFrame.other.xy;
    float2 pixelPos = input.position.xy; // SV_Position
    float2 uv = pixelPos / screenSize;

    float4 smpl_A = gBufferA.Sample(gSampler, uv);
    float4 smpl_B = gBufferB.Sample(gSampler, uv);
    float4 smpl_C = gBufferC.Sample(gSampler, uv);
    float4 smpl_D = gBufferDepth.Sample(gSampler, uv);

    float depth = smpl_D.r;
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clipPos = float4(ndcXY, depth, 1.0f);
    float4 worldPos = mul(gFrame.invViewProj, clipPos);
    worldPos /= worldPos.w;

    float3 L = normalize(-gLight.direction.xyz);
    float3 N = smpl_A.xyz * 2.0f - 1.0f; // Decode normal from [0,1] to [-1,1]
    float lambert = max(dot(N, L), 0.0f);

    float4 lightClip = mul(gLight.viewProj, worldPos);

    float lit = computeShadow(lightClip, lambert);

    float3 vtxColor = smpl_C.rgb * 0.5f + float3(0.5f);
    float3 diffuse = vtxColor * gLight.color.xyz * gLight.color.w * lambert * lit;
    float3 ambient = vtxColor * gLight.ambient.xyz * gLight.ambient.w;
    float4 color = float4(ambient + diffuse, 1.0f);
    return color;
    // return float4(uv, 0.0, 1.0); // Colorize based on UV for testing
}
