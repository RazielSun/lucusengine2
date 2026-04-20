#include "common/defines.slang"
#include "common/structs.slang"

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;
    o.position = float4(input.position, 1.0);
#if TARGET_METAL || TARGET_DX12
    o.position.y *= -1.0; // Flip Y for Metal's coordinate system
#endif
    o.color = input.color;
    return o;
}

[shader("fragment")]
float4 psMain(VSOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}