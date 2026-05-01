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

    class renderer : public singleton<renderer>
    {
    public:
        static u32 g_frameIndex;

        void add_window(window_handle handle);

        void tick(float dt);
        void cleanup();

        void setRenderMode(render_mode mode) { r_mode = mode; }
        void setCurrentScene(scene* scn) { _currentScene.reset(scn); }

        render_mode getMode() const { return r_mode; }

    protected:
        void prepareScene(const scene* scn);
        void shadowPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void forwardPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void gbufferPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void lightingPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);

        void updateFrameUniformBuffer(const uniform_buffer_handle& handle, u32 width, u32 height);
        void updateFrameUniformBuffer(const camera* cmr, u32 width, u32 height);
        void updateFrameUniformBuffer(const directional_light* dir_light, u32 width, u32 height);
        void updateLightUniformBuffer(const directional_light* dir_light);
        void updateObjectUniformBuffer(const render_object* obj);

        void initDefaultResources();

    private:
        render_mode r_mode = render_mode::FORWARD;
        std::shared_ptr<dynamic_rhi> _dynamicRHI;

        // default ub & tex
        uniform_buffer_handle g_defaultViewBufferHandle{};
        uniform_buffer_handle g_defaultObjectBufferHandle{};
        uniform_buffer_handle g_defaultLightBufferHandle{};

        sampler_handle g_defaultSamplerHandle{};
        sampler_handle g_shadowMapSamplerHandle{};

        intrusive_ptr<texture> g_defaultWhiteTexture{};
        intrusive_ptr<texture> g_defaultBlackTexture{};

        // SHADOW PASS
        u32 shadow_map_width = 1024, shadow_map_height = 1024;
        render_target_handle shadow_rt_handle;
        intrusive_ptr<material> g_shadowMat;

        render_target_handle g_gbufferA;
        render_target_handle g_gbufferB;
        render_target_handle g_gbufferC;
        render_target_handle g_gbufferDepth;
        intrusive_ptr<material> g_deferredLightingMat;
        sampler_handle g_gbufferSamplerHandle{};

        intrusive_ptr<scene> _currentScene;
    };
}
