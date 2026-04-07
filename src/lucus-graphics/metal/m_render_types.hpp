#pragma once

#include "m_pch.hpp"

#include "render_types.hpp"
#include "m_buffer.hpp"

namespace lucus
{
    class mesh;
    
    struct m_mesh
    {
        bool bHasVertexData{false};
        uint32_t vertexCount{0};
        uint32_t indexCount{0};

        m_buffer vertexBuffer;
        m_buffer indexBuffer;

        void init(id<MTLDevice> device, mesh* msh);
        void cleanup();
    };
}