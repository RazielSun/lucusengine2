#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"
#include "dx_buffer.hpp"

namespace lucus
{
    struct dx_commandbuffer_pool
    {
        public:
            void init(Com<ID3D12Device> device);
            void cleanup();
        
            std::array<Com<ID3D12CommandAllocator>, g_framesInFlight> commandAllocators{};
    };

    struct dx_viewport
    {
        D3D12_VIEWPORT viewport{};
        D3D12_RECT scissor{};
    };

    class mesh;

    struct dx_mesh
    {
        bool bHasVertexData{false};
        uint32_t vertexCount{0};
        uint32_t indexCount{0};

        dx_buffer vertexBuffer;
        dx_buffer indexBuffer;

        D3D12_VERTEX_BUFFER_VIEW vbView{};
        D3D12_INDEX_BUFFER_VIEW ibView{};

        void init(Com<ID3D12Device> device, mesh* msh);
        void cleanup();
    };
}