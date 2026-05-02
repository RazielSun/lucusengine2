#ifndef BINDLESS_TEXTURES_SLANG
#define BINDLESS_TEXTURES_SLANG

[[vk::binding(0, 8)]]
Texture2D gBindlessTextures[] : register(t2);
[[vk::binding(0, 9)]]
SamplerState gBindlessSamplers[] : register(s2);

#endif
