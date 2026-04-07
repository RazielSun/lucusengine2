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
            virtual const std::vector<window_context_handle>& getWindowContexts() const override;
            virtual float getWindowContextAspectRatio(const window_context_handle& handle) const override;

            virtual void beginFrame(const window_context_handle& handle) override;
            virtual void submit(const window_context_handle& handle, const command_buffer& cmd) override;
            virtual void endFrame(const window_context_handle& handle) override;

            virtual material_handle createMaterial(material* mat) override;
            virtual mesh_handle createMesh(mesh* msh) override;
            virtual render_object_handle createUniformBuffer(render_object* obj) override;

        protected:
            void createInstance();
            void createDevice();

            // void wait_idle();

        private:
            Com<IDXGIFactory4> _DXGIFactory;

            std::unique_ptr<dx_device> _device;
            Com<ID3D12Device> _deviceHandle;

            Com<ID3D12CommandQueue> _commandQueue;

            std::vector<dx_window_context> _contexts;
            std::vector<window_context_handle> _contextHandles;

            //
            std::unordered_map<uint64_t, dx_pipeline_state> _pipelineStates;

            //
            std::unordered_map<uint64_t, dx_mesh> _meshes;

            //
            std::vector<dx_uniform_buffer> _objectUniformBuffers;
    };
}