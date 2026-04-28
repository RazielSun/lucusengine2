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

        void init(render_mode in_mode);
        void add_window(window_handle handle);

        void tick(float dt);
        void cleanup();

        void setCurrentScene(scene* scn) { _currentScene.reset(scn); }

    protected:
        void prepareScene(const scene* scn);
        void shadowPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void forwardPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void gbufferPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void lightingPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);

        void updateFrameUniformBuffer(const camera* cmr, float aspectRatio);
        void updateFrameUniformBuffer(const directional_light* dir_light);
        void updateLightUniformBuffer(const directional_light* dir_light);
        void updateObjectUniformBuffer(const render_object* obj);

        void initDefaultResources();

    private:
        render_mode r_mode = render_mode::NONE;
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

        render_target_handle g_gbuffer_handle;

        intrusive_ptr<scene> _currentScene;
    };
}
