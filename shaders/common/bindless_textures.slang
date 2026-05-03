#ifndef BINDLESS_TEXTURES_SLANG
#define BINDLESS_TEXTURES_SLANG

// HLSL: start after deferred g-buffer (t3–t6, s3) so DX12 root tables do not overlap.
[[vk::binding(0, 8)]]
Texture2D gBindlessTextures[] : register(t7);
[[vk::binding(0, 9)]]
SamplerState gBindlessSamplers[] : register(s7);

#endif
