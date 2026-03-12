#include <metal_stdlib>

using namespace metal;

struct VSOut
{
    float4 position [[position]];
    float3 color;
};

vertex VSOut vs_main(uint vertex_id [[vertex_id]])
{
    float2 positions[3] = {
        float2( 0.0,  0.5),
        float2(-0.5, -0.5),
        float2( 0.5, -0.5)
    };

    float3 colors[3] = {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };

    VSOut out;
    out.position = float4(positions[vertex_id], 0.0, 1.0);
    out.color = colors[vertex_id];
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]])
{
    return float4(in.color, 1.0);
}