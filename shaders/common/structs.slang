#ifndef STRUCTS_SLANG
#define STRUCTS_SLANG

struct FrameUniforms
{
    float4x4 view;
    float4x4 proj;
};

struct ObjectUniforms
{
    float4x4 model;
};

struct VSInput {
    float3 position : LOCATION0;
    float3 color : LOCATION1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

#endif /* STRUCTS_SLANG */