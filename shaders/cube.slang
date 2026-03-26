#include "common/defines.slang"

struct FrameUniforms
{
    // float4x4 model; // TEST ONLY
    float4x4 view;
    float4x4 proj;
};

struct ObjectUniforms
{
    float4x4 model;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> gFrame : register(b0);
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUniforms> gObject: register(b1);

struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("vertex")]
VSOutput vsMain(uint vertexID : SV_VertexID)
{
    static const float3 positions[8] =
    {
        float3(-0.5, -0.5, -0.5), // 0
        float3( 0.5, -0.5, -0.5), // 1
        float3( 0.5,  0.5, -0.5), // 2
        float3(-0.5,  0.5, -0.5), // 3
        float3(-0.5, -0.5,  0.5), // 4
        float3( 0.5, -0.5,  0.5), // 5
        float3( 0.5,  0.5,  0.5), // 6
        float3(-0.5,  0.5,  0.5), // 7
    };

    static const uint indices[36] =
    {
        0, 1, 2,  2, 3, 0, // back
        4, 5, 6,  6, 7, 4, // front
        0, 4, 7,  7, 3, 0, // left
        1, 5, 6,  6, 2, 1, // right
        3, 2, 6,  6, 7, 3, // top
        0, 1, 5,  5, 4, 0  // bottom
    };

    static const float3 colors[8] =
    {
        float3(1,0,0),
        float3(0,1,0),
        float3(0,0,1),
        float3(1,1,0),
        float3(1,0,1),
        float3(0,1,1),
        float3(1,1,1),
        float3(0,0,0)
    };

    uint i = indices[vertexID];
    float3 localPos = positions[i];
    float3 color = colors[i];

    float4 worldPos = mul(gObject.model, float4(localPos, 1.0));
    // float4 worldPos = mul(gFrame.model, float4(localPos, 1.0));
    float4 viewPos  = mul(gFrame.view, worldPos);
    float4 clipPos  = mul(gFrame.proj, viewPos);

    VSOutput o;
    o.position = clipPos;
    o.color = color;
    return o;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}