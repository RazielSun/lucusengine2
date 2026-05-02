#pragma once

#include "m_pch.hpp"

#include "dynamic_rhi.hpp"
#include "m_render_types.hpp"
#include "m_device.hpp"
#include "m_buffer.hpp"
#include "m_window_context.hpp"
#include "m_pipeline_state.hpp"

namespace lucus
{
    class m_dynamic_rhi : public dynamic_rhi
    {
        public:
            m_dynamic_rhi();
            virtual ~m_dynamic_rhi() override;

            virtual void init() override;

            virtual window_context_handle createWindowContext(const window_handle& handle) override;
            virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const override;

            virtual pipeline_state_handle createPSO(material* mat) override;
            virtual mesh_handle createMesh(mesh* msh) override;

            virtual sampler_handle createSampler(resource_binding_mode binding_mode = resource_binding_mode::BINDFULL) override;
            virtual texture_handle createTexture(texture* tex) override;
            virtual void loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex) override;

            virtual uniform_buffer_handle createUniformBuffer(size_t bufferSize, shader_binding_stage stage = shader_binding_stage::VERTEX) override;
            virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) override;

            virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd) override;
        
        protected:
            void createDevice();

            // void createObjectUniformBuffers();

            void uploadTextureToGpu(m_texture& tex);

        private:
            std::unique_ptr<m_device> _device;
            id<MTLDevice> _deviceHandle;

            std::vector<m_window_context> _contexts;

            //
            std::unordered_map<u32, m_pipeline_state> _pipelineStates;

            //
            std::unordered_map<u32, m_mesh> _meshes;

            //
            std::vector<m_sampler> _samplers;
            std::unordered_map<u32, m_texture> _textures;

            //
            std::vector<m_buffer> _uniformBuffers;
            
            // std::array<m_buffer, g_framesInFlight> _objectUniformBuffers;
    };
}