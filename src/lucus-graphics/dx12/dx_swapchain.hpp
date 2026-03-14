#pragma once

#include "dx_pch.hpp"

#include "swapchain.hpp"

namespace lucus
{
    class dx_swapchain : public iswapchain
    {
        public:
            dx_swapchain() = default;
            virtual ~dx_swapchain();

            void setContext(Com<IDXGIFactory4> factory, Com<ID3D12CommandQueue> commandQueue);

            virtual void createSurface(std::shared_ptr<window> window) override;
            virtual void create(std::shared_ptr<window> window) override;

            Com<IDXGISwapChain3> getSwapChain() const { return mSwapChain; }

            D3D12_VIEWPORT mViewport = {};
            D3D12_RECT mScissorRect = {};

        private:
            Com<IDXGIFactory4> mDXGIFactory;
            Com<ID3D12CommandQueue> mCommandQueue;

            HWND _hwnd;

            Com<IDXGISwapChain3> mSwapChain;
    };
}