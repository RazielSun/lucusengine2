#ifndef STRUCTS_SLANG
#define STRUCTS_SLANG

struct FrameUniforms
{
    float4x4 view;
    float4x4 proj;
    float4x4 invViewProj;
    float4 position;
    float4 other; // xy - screen size
};

struct ObjectUniforms
{
    float4x4 model;
};

struct MaterialUniforms
{
    float4 baseColor;
    float4 materialParams;
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

struct GBufferOutput
{
    float4 normal_flags : SV_Target0; // NORMAL + Flags
    float4 met_rgh_spc_shd : SV_Target1; // Metalness + Roughness + Specular + ShadingModel
    float4 albedo_ao : SV_Target2; // ALBEDO + AO
};

#endif /* STRUCTS_SLANG */
