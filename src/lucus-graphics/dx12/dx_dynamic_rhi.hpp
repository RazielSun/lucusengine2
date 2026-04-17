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
            virtual texture_handle loadTexture(texture* tex) override;

            virtual uniform_buffer_handle createUniformBuffer(uniform_buffer_type ub_type, size_t bufferSize) override;
            virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) override;

            virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd) override;

        protected:
            void createInstance();
            void createDevice();

            // void wait_idle();
            void initTextureCmdResources();
            void uploadTextureToGpu(dx_texture& tex);

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

            // upload texture, need refactor
            Com<ID3D12CommandAllocator> _texCmdAllocator;
            Com<ID3D12GraphicsCommandList> _texCmdBuffer;
            Com<ID3D12Fence> _texFence;
            uint64_t _texFenceValue{0};
            void* _texFenceEvent = nullptr;

            
    };
}