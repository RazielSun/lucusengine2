#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"
#include "dx_buffer.hpp"

#include "texture.hpp"
#include "intrusive_ptr.hpp"

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

        u32 index;
        D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle;
        

        UINT bytesPerPixel;
        UINT64 width;
        UINT64 height;
        UINT64 texSize;

        intrusive_ptr<texture> texLink;

        void init(Com<ID3D12Device> device, Com<ID3D12DescriptorHeap> srvHeap, u32 in_index, texture* tex);
        void free_staging();
        void cleanup();
    };

    struct dx_sampler
    {
        u32 index;
        D3D12_CPU_DESCRIPTOR_HANDLE samplerCPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE samplerGPUHandle;

        void init(Com<ID3D12Device> device, Com<ID3D12DescriptorHeap> samplerHeap, u32 in_index);
        void cleanup();
    };

    struct dx_allocator
    {
        u32 initial;
        u32 capacity;

        u32 head;
        u32 current;

        dx_allocator() {}

        void init(u32 in_initial, u32 in_capacity) { initial = in_initial; capacity = in_capacity; }
        void reset() { head = initial; current = initial; }
        void head_to_current() { head = current; }
        u32 allocate() { assert(current < initial + capacity); return current++; }
    };

    struct dx_heap_descriptor
    {
        Com<ID3D12DescriptorHeap> resourceHeap;
        Com<ID3D12DescriptorHeap> shaderHeap;

        std::array<dx_allocator, g_framesInFlight> allocators{};

        u32 resourceIndex = 0;
        u32 heapItemSize = 0;
        u32 capacity = 0;
        D3D12_DESCRIPTOR_HEAP_TYPE heapType;

        void init(Com<ID3D12Device> device, u32 in_capacity, D3D12_DESCRIPTOR_HEAP_TYPE type);
        u32 allocateResource(u32 count = 1);
        CD3DX12_CPU_DESCRIPTOR_HANDLE getResourceHandle(u32 index) const;
        CD3DX12_GPU_DESCRIPTOR_HANDLE getShaderHeadHandle(u32 frameIndex, u32 offset = 0) const;
        void copy(u32 index, u32 frameIndex);
        void reset_allocator(u32 frameIndex);
        void head_to_current(u32 frameIndex);
        void cleanup();
    private:
        Com<ID3D12Device> _device;
    };

    struct dx_images_desc
    {
        u32 count {0};
        u32 width {0};
        u32 height {0};

        DXGI_FORMAT format {DXGI_FORMAT_UNKNOWN};
        DXGI_FORMAT shaderReadFormat {DXGI_FORMAT_UNKNOWN};
        D3D12_DESCRIPTOR_HEAP_TYPE heapType {D3D12_DESCRIPTOR_HEAP_TYPE_RTV};
        D3D12_RESOURCE_FLAGS resourceFlags {D3D12_RESOURCE_FLAG_NONE};
        D3D12_RESOURCE_STATES initialState {D3D12_RESOURCE_STATE_COMMON};
        D3D12_CLEAR_VALUE clearValue{};

        bool bIsColor {true};
        bool bPreinitialized {false};

        bool bShaderRead {false};
        Com<ID3D12DescriptorHeap> resourceHeap { nullptr };
        u32 ShaderHeadIndex {0};
    };

    struct dx_images
    {
        bool bPreinitialized { false };
        bool bShaderRead { false };
        u32 ShaderHeadIndex {0};
        D3D12_RESOURCE_STATES initialState { D3D12_RESOURCE_STATE_COMMON };
        Com<ID3D12DescriptorHeap> heap;
        u32 descriptorSize = 0;
        std::vector<Com<ID3D12Resource>> images;
        std::vector<D3D12_RESOURCE_STATES> states;

        void init(Com<ID3D12Device> device, const dx_images_desc& init_desc);
        void cleanup();

        CD3DX12_CPU_DESCRIPTOR_HANDLE getResourceHandle(u32 index) const;
        u32 getShaderHandle(u32 index) const;
    private:
        Com<ID3D12Device> _device;
    };

    struct dx_render_target_desc
    {
        u32 count {0};
        u32 width {0};
        u32 height {0};

        bool bSwapChain {false};

        bool bUseColor {false};
        DXGI_FORMAT colorFormat {DXGI_FORMAT_R8G8B8A8_UNORM};
        bool bColorShaderRead {false};
        u32 ColorSRVHeadIndex {0};

        bool bUseDepth {false};
        DXGI_FORMAT depthFormat {DXGI_FORMAT_D32_FLOAT};
        bool bDepthShaderRead {false};
        u32 DepthSRVHeadIndex {0};

        Com<ID3D12DescriptorHeap> resourceHeap { nullptr };
    };

    struct dx_render_target
    {
        bool bColor { true };
        dx_images color;

        bool bDepth { false };
        dx_images depth;

        bool bSwapChain {false};

        void init(Com<ID3D12Device> device, const dx_render_target_desc& init_desc);
        void cleanup();
    };
}
