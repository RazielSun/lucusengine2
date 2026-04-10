#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"
#include "dx_buffer.hpp"

namespace lucus
{
    class mesh;
    class texture;
    class material;

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

    struct dx_mesh : public rhi_mesh
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
        ComPtr<ID3D12Resource> stgBuffer;
        ComPtr<ID3D12Resource> texResource;
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

    struct dx_texture_bind
    {
        Com<ID3D12DescriptorHeap> srvHeap;
        Com<ID3D12DescriptorHeap> samplerHeap;
        UINT slot;
    };

    struct dx_material : public rhi_material
    {
        std::vector<dx_texture_bind> tex_binds;

        void init(Com<ID3D12Device> device, material* mat);
        void cleanup();
    };
}