#pragma once

#include "pch.hpp"

#include "core_types.hpp"
#include "render_types.hpp"

namespace lucus
{
    class window;
    class material;
    class mesh;
    class texture;
    class render_object;
    struct gpu_command_buffer;
    struct image_capture;

    std::shared_ptr<class dynamic_rhi> create_dynamic_rhi();

    class dynamic_rhi
    {
    public:
        dynamic_rhi() = default;
        virtual ~dynamic_rhi() = default;

        virtual void init(const rhi_init_desc& desc) = 0;

        const validation_status& getValidationStatus() const { return _validationStatus; }

        virtual window_context_handle createWindowContext(const window_handle& handle) = 0;
        virtual const std::vector<window_context_handle>& getWindowContexts() const { return _contextHandles; }
        virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const = 0;
        virtual render_target_handle getWindowContextRenderTarget(const window_context_handle& handle, render_target_type rt_type) const = 0;

        /// Invalid handles if the backend has no G-buffer support or the context is bad.
        virtual const window_gbuffer_targets& getWindowContextGBufferTargets(const window_context_handle& /*handle*/) const
        {
            static const window_gbuffer_targets kEmpty{};
            return kEmpty;
        }

        /// No-op if already allocated or the extent is zero.
        virtual void ensureDeferredGBufferTargets(const window_context_handle& /*ctx*/) {}

        virtual pipeline_state_handle createPSO(material* mat, render_pass_name passName) = 0;
        virtual render_target_handle createRenderTarget(u32 width, u32 height, render_target_type rt_type, u32 count = 1, bool skip_resources = false) = 0;
        
        virtual mesh_handle createMesh(mesh* msh) = 0;

        virtual sampler_handle createSampler(const sampler_desc& desc = {}, resource_binding_mode binding_mode = resource_binding_mode::BINDFULL) = 0;
        virtual texture_handle createTexture(texture* tex) = 0;
        virtual void loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex) = 0;

        virtual uniform_buffer_handle createUniformBuffer(size_t bufferSize, shader_binding_stage stage = shader_binding_stage::VERTEX) = 0;
        virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) = 0;

        virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd, image_capture* capture = nullptr) = 0;

    protected:
        void beginValidationSetup(const rhi_init_desc& desc);
        void finishValidationSetup();

        std::vector<window_context_handle> _contextHandles;

        rhi_init_desc _initDesc;
        validation_status _validationStatus;
    };
}
