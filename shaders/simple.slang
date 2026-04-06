#include "common/defines.slang"
#include "common/structs.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject: register(b1);

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    float4 worldPos = mul(gObject.model, float4(input.position, 1.0));
    float4 viewPos  = mul(gFrame.view, worldPos);
    float4 clipPos  = mul(gFrame.proj, viewPos);

    VSOutput o;
    o.position = clipPos;
    o.color = input.color;
    return o;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}