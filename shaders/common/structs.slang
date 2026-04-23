#ifndef STRUCTS_SLANG
#define STRUCTS_SLANG

struct FrameUniforms
{
    float4 position;
    float4x4 view;
    float4x4 proj;
};

struct ObjectUniforms
{
    float4x4 model;
};

struct LightUniforms
{
    float4 position;
    float4 direction;
    float4 color;   // xyz - color, w - intensity
    float4 ambient; // xyz - color, w - intensity
    float4x4 viewProj;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoords0 : TEXCOORD0;
    float3 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoords0 : TEXCOORD0;
    float3 color : COLOR0;
    float4 lightClipPos : TEXCOORD1;
};

#endif /* STRUCTS_SLANG */
