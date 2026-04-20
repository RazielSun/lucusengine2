#include "common/defines.slang"
#include "common/structs.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject : register(b1);
[[vk::binding(0, 2)]]
ConstantBuffer<LightUniforms> gLight : register(b2);

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    float4 worldPos = mul(gObject.model, float4(input.position, 1.0));
    float4 viewPos = mul(gFrame.view, worldPos);
    float4 clipPos = mul(gFrame.proj, viewPos);

    VSOutput o;
    o.position = clipPos;
    o.texCoords0 = input.texCoords0;
    o.color = input.color;
    return o;
}

[[vk::binding(0, 3)]]
Texture2D gAlbedo : register(t0);
[[vk::binding(0, 4)]]
SamplerState gSampler : register(s0);

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    float4 color = gAlbedo.Sample(gSampler, input.texCoords0);
    color.rgb *= input.color;
    return color;
}