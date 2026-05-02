#include "common/defines.slang"
#include "common/structs.slang"
#include "common/bindless_textures.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject : register(b1);
[[vk::binding(0, 2)]]
ConstantBuffer<MaterialUniforms> gMaterial : register(b2);

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

[[vk::binding(0, 4)]]
Texture2D gAlbedo : register(t0);
[[vk::binding(0, 6)]]
SamplerState gSampler : register(s0);

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    static const uint kMaterialTexHasAlbedo = 1u;
    static const uint kMaterialTexBindlessAlbedo = 2u;

    float4 color;
    const uint matTexFlags = gMaterial.textureInfo.x;
    if ((matTexFlags & kMaterialTexBindlessAlbedo) != 0u && (matTexFlags & kMaterialTexHasAlbedo) != 0u)
    {
        uint idx = gMaterial.textureInfo.y;
        color = gBindlessTextures[NonUniformResourceIndex(idx)].Sample(gBindlessSamplers[NonUniformResourceIndex(0u)], input.texCoords0);
    }
    else if ((matTexFlags & kMaterialTexHasAlbedo) != 0u)
    {
        color = gAlbedo.Sample(gSampler, input.texCoords0);
    }
    else
    {
        color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    color.rgb *= input.color * gMaterial.baseColor.rgb;
    return color;
}
