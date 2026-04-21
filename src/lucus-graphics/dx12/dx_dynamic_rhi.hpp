#pragma once

#include "dx_pch.hpp"

#include "dynamic_rhi.hpp"
#include "dx_device.hpp"
#include "dx_window_context.hpp"
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

            virtual window_context_handle createWindowContext(const window_handle& handle) override;
            virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const override;

            virtual pipeline_state_handle createPSO(material* mat) override;
            virtual mesh_handle createMesh(mesh* msh) override;

            virtual sampler_handle createSampler() override;
            virtual texture_handle createTexture(texture* tex) override;
            virtual void loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex) override;

            virtual uniform_buffer_handle createUniformBuffer(size_t bufferSize, shader_binding_stage stage = shader_binding_stage::VERTEX) override;
            virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) override;

            virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd) override;

        protected:
            void createInstance();
            void createDevice();
            void createCommandBufferPool();
            void createSyncObjects();
            void createDescriptorHeaps();

            // void wait_idle();
            void uploadTextureToGpu(dx_texture& tex, u32 frameIndex);

        private:
            Com<IDXGIFactory4> _DXGIFactory;

            std::unique_ptr<dx_device> _device;
            Com<ID3D12Device> _deviceHandle;

            Com<ID3D12CommandQueue> _commandQueue;

            std::vector<dx_window_context> _contexts;

            //
            std::unordered_map<u32, dx_pipeline_state> _pipelineStates;

            //
            std::unordered_map<u32, dx_mesh> _meshes;

            //
            std::vector<dx_uniform_buffer> _uniformBuffers;

            //
            std::unordered_map<u32, dx_texture> _textures;
            std::vector<dx_sampler> _samplers;

            dx_heap_descriptor srvHeapDesc;
            dx_heap_descriptor samplerHeapDesc;

            //
            dx_commandbuffer_pool commandPool;
            Com<ID3D12GraphicsCommandList> commandBuffer;

            //
            Com<ID3D12Fence> fence;
            std::array<uint64_t, g_framesInFlight> fenceValues{};
            void* fenceEvent = nullptr;
    };
}
