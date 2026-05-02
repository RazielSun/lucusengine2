#include "common/defines.slang"
#include "common/structs.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject : register(b1);
[[vk::binding(0, 2)]]
ConstantBuffer<MaterialUniforms> gMaterial : register(b2);

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
    return o;
}

[shader("fragment")]
GBufferOutput psMain(VSOutput input)
{
    float3 N = (normalize(input.normal) + 1.0f) * 0.5f;
    float3 vtxColor = input.color * gMaterial.baseColor.rgb;

    GBufferOutput o;
    o.normal_flags = float4(N, 0.0f);
    o.met_rgh_spc_shd = float4(0.0f, 1.0f, 0.0f, 0.0f);
    o.albedo_ao = float4(vtxColor, 1.0f);
    return o;
}
