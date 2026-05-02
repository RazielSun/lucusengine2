#ifndef BINDLESS_TEXTURES_SLANG
#define BINDLESS_TEXTURES_SLANG

[[vk::binding(0, 8)]]
Texture2D gBindlessTextures[] : register(t0, space8);
[[vk::binding(0, 9)]]
SamplerState gBindlessSamplers[] : register(s0, space9);

#endif
