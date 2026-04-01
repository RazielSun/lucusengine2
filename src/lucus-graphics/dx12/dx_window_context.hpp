#pragma once

#include "dx_pch.hpp"

#include "dx_render_types.hpp"
#include "dx_buffer.hpp"

namespace lucus
{
    class window;
    
    struct dx_window_context
    {
        void init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, window* window);
        void cleanup();

        dx_viewport viewport;

        Com<ID3D12CommandQueue> mCommandQueue;

        Com<IDXGISwapChain3> swapChain;

        Com<ID3D12DescriptorHeap> mRTVHeap;
        uint32_t mRTVDescriptorSize = 0;

        std::array<Com<ID3D12Resource>, g_swapchainImageCount> mRenderTargets{};

        uint32_t backBufferIndex{ 0 };

        Com<ID3D12Fence> fence;
        std::array<uint64_t, g_framesInFlight> fenceValues{};
        void* fenceEvent = nullptr;

        dx_buffer uniformbuffers;

        Com<ID3D12DescriptorHeap> mDSVHeap;
        uint32_t mDSVDescriptorSize = 0;
        
        std::array<Com<ID3D12Resource>, g_framesInFlight> mDepthStencils{};
        DXGI_FORMAT mDepthFormat = DXGI_FORMAT_D32_FLOAT;

        protected:
            void createRTVHeaps();
            void createDSVHeap();
            void createRenderTargets();
            void createDepthStencils();
            void createSyncObjects();
        
        private:
            Com<IDXGIFactory4> mDXGIFactory;
            Com<ID3D12Device> mDevice;
            
    };
}