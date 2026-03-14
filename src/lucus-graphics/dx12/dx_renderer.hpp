#pragma once

#include "dx_pch.hpp"

#include "renderer.hpp"
#include "dx_device.hpp"
#include "dx_swapchain.hpp"

namespace lucus
{
    class dx_renderer : public renderer
    {
        public:
            dx_renderer();
            ~dx_renderer() override = default;

            virtual bool init() override;
            virtual bool prepare(std::shared_ptr<window> window) override;
            virtual void tick() override;
            virtual void cleanup() override;

        protected:
            void createInstance();
            void initDevices();

            void createCommandBuffers();
            void destroyCommandBuffers();

            void createSyncObjects();
            void destroySyncObjects();

            void createRTVHeaps();
            void createRenderTargets();

            void createRootSignature();

            void createGraphicsPipeline();

            void prepareFrame();
            void buildCommandBuffer();
            void submitFrame();

        private:
            Com<IDXGIFactory4> mDXGIFactory;

            std::unique_ptr<dx_device> _device;
            Com<ID3D12Device> deviceDX;

            std::array<Com<ID3D12CommandAllocator>, g_maxConcurrentFrames> mCommandAllocators{};

            Com<ID3D12CommandQueue> mCommandQueue;

            std::unique_ptr<dx_swapchain> _swapchain;

            Com<ID3D12GraphicsCommandList> mCommandBuffer;

            uint32_t _currentFrame{ 0 };
            Com<ID3D12Fence> mFence;
            std::array<uint64_t, g_maxConcurrentFrames> mFenceValues{};
            void* mFenceEvent = nullptr;

            Com<ID3D12RootSignature> mRootSignature;

            Com<ID3D12PipelineState> mPipelineState;

            Com<ID3D12DescriptorHeap> mRTVHeap;
            uint32_t mRTVDescriptorSize = 0;

            std::array<Com<ID3D12Resource>, g_maxConcurrentFrames> mRenderTargets{};
            // D3D12_VIEWPORT mViewport = {};
            // D3D12_RECT mScissorRect = {};
        };
}