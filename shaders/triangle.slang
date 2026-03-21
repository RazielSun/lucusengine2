struct VSOutput
{
    float4 position : SV_Position;
    float3 fragColor : TEXCOORD0;
};

[shader("vertex")]
VSOutput vsMain(uint vertexIndex : SV_VertexID)
{
    float2 positions[3] =
    {
        float2( 0.0, -0.5),
        float2( 0.5,  0.5),
        float2(-0.5,  0.5)
    };

    float3 colors[3] =
    {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };

    VSOutput o;
    o.position = float4(positions[vertexIndex], 0.0, 1.0);
    o.fragColor = colors[vertexIndex];
    return o;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target
{
    return float4(input.fragColor, 1.0);
}