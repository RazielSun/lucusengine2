#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"
#include "dx_buffer.hpp"

namespace lucus
{
    class mesh;
    class texture;

    struct dx_commandbuffer_pool
    {
        public:
            void init(Com<ID3D12Device> device);
            void cleanup();
        
            std::array<Com<ID3D12CommandAllocator>, g_framesInFlight> commandAllocators{};
    };

    struct dx_mesh : public gpu_mesh
    {
        dx_buffer vertexBuffer;
        dx_buffer indexBuffer;

        D3D12_VERTEX_BUFFER_VIEW vbView{};
        D3D12_INDEX_BUFFER_VIEW ibView{};

        void init(Com<ID3D12Device> device, mesh* msh);
        void cleanup();
    };

    struct dx_texture
    {
        Com<ID3D12Resource> stgBuffer;
        Com<ID3D12Resource> texResource;
        Com<ID3D12DescriptorHeap> srvHeap;
        Com<ID3D12DescriptorHeap> samplerHeap;

        UINT bytesPerPixel;
        UINT64 width;
        UINT64 height;
        UINT64 texSize;

        void* tex_ptr;

        void init(Com<ID3D12Device> device, texture* tex);
        void free_staging();
        void cleanup();
    };
}