#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class window;
    
    struct dx_viewport
    {
        void init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, window* window);
        void cleanup();

        D3D12_VIEWPORT mViewport = {};
        D3D12_RECT mScissorRect = {};

        Com<IDXGIFactory4> mDXGIFactory;
        Com<ID3D12Device> mDevice
        Com<ID3D12CommandQueue> mCommandQueue;

        Com<IDXGISwapChain3> mSwapChain;

        Com<ID3D12DescriptorHeap> mRTVHeap;
        uint32_t mRTVDescriptorSize = 0;

        std::array<Com<ID3D12Resource>, g_swapchainImageCount> mRenderTargets{};

        protected:
            void createRTVHeaps();
            void createRenderTargets();
    };
}