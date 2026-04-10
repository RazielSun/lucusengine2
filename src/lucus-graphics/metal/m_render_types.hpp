#pragma once

#include "m_pch.hpp"

#include "render_types.hpp"
#include "m_buffer.hpp"

namespace lucus
{
    class mesh;
    class material;
    class texture;
    
    struct m_mesh : public rhi_mesh
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
        id<MTLSamplerState> sampler;

        NSUInteger width;
        NSUInteger height;
        NSUInteger texSize;
        NSUInteger bytesPerPixel;
        NSUInteger bytesPerRow;

        void init(id<MTLDevice> device, texture* tex);
        void cleanup();
    };

    struct m_texture_bind
    {
        id<MTLTexture> mtexture;
        id<MTLSamplerState> sampler;
        NSUInteger slot;
    };

    struct m_material : public rhi_material
    {
        std::vector<m_texture_bind> texture_binds;

        void init(id<MTLDevice> device, material* mat);
        void cleanup();
    };
}