#pragma once

#include "dx_pch.hpp"

#include "dx_render_types.hpp"

namespace lucus
{
    class window;
    
    struct dx_window_context
    {
        void init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, DXGI_FORMAT format, window* window);
        void init_images(dx_render_target& rt);
        void cleanup();

        Com<ID3D12CommandQueue> mCommandQueue;

        Com<IDXGISwapChain3> swapChain;

        u32 imageCount {0};
        u32 width {0};
        u32 height {0};

        uint32_t backBufferIndex{ 0 };

        render_target_handle color_handle;
        render_target_handle depth_handle;

        window_gbuffer_targets gbuffer{};

        private:
            Com<IDXGIFactory4> mDXGIFactory;
            Com<ID3D12Device> mDevice;
            
    };
}
