#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "render_types.hpp"
#include "scene.hpp"
#include "texture.hpp"
#include "material.hpp"

namespace lucus
{
    class dynamic_rhi;
    class camera;
    struct gpu_command_buffer;
    struct image_capture;

    struct pass_context
    {
        window_context_handle ctx;
        u32 width { 0 };
        u32 height { 0 };
        render_target_handle color;
        render_target_handle depth;
        uniform_buffer_handle view;
    };

    class renderer : public singleton<renderer>
    {
    public:
        static u32 g_frameIndex;

        void add_window(window_handle handle);

        void setRHIInitDesc(const rhi_init_desc& desc) { _rhiInitDesc = desc; }
        const validation_status* getValidationStatus() const;

        void tick(float dt, image_capture* capture = nullptr);
        void cleanup();

        void setRenderMode(render_mode mode) { r_mode = mode; }
        void setCurrentScene(scene* scn) { _currentScene.reset(scn); }

        render_mode getMode() const { return r_mode; }

    protected:
        void uploadPendingTextures();

        void prepareScene(const scene* scn);
        void shadowPass(const scene* scn, gpu_command_buffer& cmd);
        void forwardPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd);
        void gbufferPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd);
        void lightingPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd);

        pass_context makePassContext(const window_context_handle& ctx_handle);
        const window_gbuffer_targets& requireGBufferTargets(const pass_context& pc, const char* pass_name);

        texture_handle baseColorTextureFor(const material* mat) const;

        uniform_buffer_handle viewBufferFor(const window_context_handle& ctx_handle);
        uniform_buffer_handle ensureViewBuffer(const camera* cmr, const pass_context& pc);
        uniform_buffer_handle ensureShadowViewBuffer(const directional_light* dir_light, u32 width, u32 height);
        uniform_buffer_handle ensureLightBuffer(const directional_light* dir_light);

        void updateFrameUniformBuffer(const uniform_buffer_handle& handle, u32 width, u32 height);
        void updateFrameUniformBuffer(const camera* cmr, const uniform_buffer_handle& handle, u32 width, u32 height);
        void updateFrameUniformBuffer(const directional_light* dir_light, u32 width, u32 height);
        void updateLightUniformBuffer(const directional_light* dir_light);
        void updateObjectUniformBuffer(const render_object* obj);
        void updateMaterialUniformBuffer(const material* mat);

        void initDefaultResources();

    private:
        render_mode r_mode = render_mode::FORWARD;
        rhi_init_desc _rhiInitDesc{};
        std::shared_ptr<dynamic_rhi> _dynamicRHI;

        std::vector<uniform_buffer_handle> _viewBuffers;

        uniform_buffer_handle g_defaultViewBufferHandle{};
        uniform_buffer_handle g_defaultObjectBufferHandle{};
        uniform_buffer_handle g_defaultMaterialBufferHandle{};
        uniform_buffer_handle g_defaultLightBufferHandle{};

        sampler_handle g_defaultSamplerHandle{};
        sampler_handle g_defaultBindlessSamplerHandle{};
        sampler_handle g_shadowMapSamplerHandle{};

        intrusive_ptr<texture> g_defaultWhiteTexture{};
        intrusive_ptr<texture> g_defaultBlackTexture{};
        intrusive_ptr<texture> g_bindlessFallbackBlackTexture{};
        intrusive_ptr<texture> g_bindlessFallbackWhiteTexture{};

        u32 shadow_map_width = 1024, shadow_map_height = 1024;
        render_target_handle shadow_rt_handle;
        intrusive_ptr<material> g_shadowMat;

        intrusive_ptr<material> g_deferredLightingMat;
        sampler_handle g_gbufferSamplerHandle{};

        intrusive_ptr<scene> _currentScene;
    };
}
