#include "common/defines.slang"
#include "common/structs.slang"
#include "common/light.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject : register(b1);
[[vk::binding(0, 2)]]
ConstantBuffer<LightUniforms> gLight : register(b2);

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    float4 worldPos = mul(gObject.model, float4(input.position, 1.0f));
    float4 normal = mul(gObject.model, float4(input.normal, 0.0f));

    float4 viewPos = mul(gFrame.view, worldPos);
    float4 clipPos = mul(gFrame.proj, viewPos);

    VSOutput o;
    o.position = clipPos;
    o.normal = normalize(normal.xyz);
    o.texCoords0 = input.texCoords0;
    o.color = input.color;
    o.lightClipPos = mul(gLight.viewProj, worldPos);
    return o;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    float3 N = normalize(input.normal);
    float3 L = normalize(-gLight.direction.xyz);
    float lambert = max(dot(N, L), 0.0f);

    float lit = computeShadow(input.lightClipPos, lambert);

    float3 vtxColor = input.color * 0.5f + float3(0.5f);
    float3 diffuse = vtxColor * gLight.color.xyz * gLight.color.w * lambert * lit;
    float3 ambient = vtxColor * gLight.ambient.xyz * gLight.ambient.w;
    float4 color = float4(ambient + diffuse, 1.0f);

    return color;
}
