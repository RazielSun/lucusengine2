#pragma once

#include "m_pch.hpp"

#include "render_types.hpp"
#include "m_buffer.hpp"

namespace lucus
{
    class mesh;
    class material;
    class texture;

    constexpr NSUInteger g_metalVertexBufferIndex{ 30 };
    constexpr size_t g_metalConstantBufferOffsetAlignment{ 256 };
    constexpr MTLPixelFormat g_metalColorTargetFormat{ MTLPixelFormatRGBA8Unorm };
    constexpr MTLPixelFormat g_metalDepthTargetFormat{ MTLPixelFormatDepth32Float };

    struct m_mesh : public gpu_mesh
    {
        m_buffer vertexBuffer;
        m_buffer indexBuffer;

        void init(id<MTLDevice> device, mesh* msh);
        void cleanup();
    };

    struct m_texture
    {
        id<MTLBuffer> stgBuffer;
        id<MTLTexture> mtexture;

        NSUInteger width;
        NSUInteger height;
        NSUInteger texSize;
        NSUInteger bytesPerPixel;
        NSUInteger bytesPerRow;

        void init(id<MTLDevice> device, texture* tex);
        void free_staging();
        void cleanup();
    };

    struct m_sampler
    {
        id<MTLSamplerState> sampler;

        void init(id<MTLDevice> device, const sampler_desc& desc);
        void cleanup();
    };

    struct m_render_target
    {
        render_target_type type{ render_target_type::NONE };
        bool swapchain{ false };
        std::vector<id<MTLTexture>> textures;

        void init(id<MTLDevice> device, u32 width, u32 height, render_target_type rt_type, u32 count);
        void initSwapchain();
        id<MTLTexture> texture(u32 frameIndex) const;
        void cleanup();
    };
}