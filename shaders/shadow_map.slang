#include "common/defines.slang"
#include "common/structs.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);

[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject : register(b1);

struct ShadowVSOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
ShadowVSOutput vsMain(VSInput input)
{
    float4 worldPos = mul(gObject.model, float4(input.position, 1.0f));
    float4 clipPos = mul(gFrame.proj, mul(gFrame.view, worldPos));

    ShadowVSOutput output;
    output.position = clipPos;
    return output;
}

[shader("fragment")]
void psMain(ShadowVSOutput input)
{
    // Intentionally empty.
    // Depth is written by fixed-function depth output from SV_Position.
    // No SV_Target0 here because SHADOW_PASS has no color attachments.
}