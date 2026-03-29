#pragma once

#include "dx_pch.hpp"

#include "dynamic_rhi.hpp"
#include "dx_device.hpp"
#include "dx_viewport.hpp"
#include "dx_pipeline_state.hpp"
#include "dx_buffer.hpp"
#include "dx_render_types.hpp"

namespace lucus
{
    class dx_dynamic_rhi : public dynamic_rhi
    {
        public:
            dx_dynamic_rhi();
            virtual ~dx_dynamic_rhi();

            virtual void init() override;

            virtual viewport_handle createViewport(const window_handle& handle) override;

            virtual void beginFrame(const viewport_handle& handle) override;
            virtual void endFrame() override;

            virtual void submit(const command_buffer& cmd) override;

            virtual material_handle createMaterial(material* mat) override;

            virtual render_object_handle createUniformBuffer(render_object* obj) override;

        protected:
            void createInstance();
            void createDevice();
            void createCommandBufferPool();

            void createSyncObjectsStable();

            void createFrameUniformBuffers();

            void wait_idle();

        private:
            Com<IDXGIFactory4> _DXGIFactory;

            std::unique_ptr<dx_device> _device;
            Com<ID3D12Device> _deviceHandle;

            dx_commandbuffer_pool _commandbuffer_pool;
            Com<ID3D12GraphicsCommandList> _commandBuffer;

            Com<ID3D12CommandQueue> _commandQueue;

            viewport_handle _currentViewport;
            uint32_t _currentFrame{ 0 };

            Com<ID3D12Fence> _fence;
            std::array<uint64_t, g_framesInFlight> _fenceValues{};
            void* _fenceEvent = nullptr;

            std::vector<dx_viewport> _viewports;

            //
            std::unordered_map<uint32_t, dx_pipeline_state> _pipelineStates;

            //
            dx_buffer _frameUniformBuffer;
            std::vector<dx_buffer> _objectUniformBuffers;
    };
}