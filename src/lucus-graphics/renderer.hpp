#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "render_types.hpp"
#include "scene.hpp"
#include "texture.hpp"

namespace lucus
{
    class dynamic_rhi;
    class camera;
    struct gpu_command_buffer;

    class renderer : public singleton<renderer>
    {
    public:
        static u32 g_frameIndex;

        void init(window_handle handle);

        void tick(float dt);
        void cleanup();

        void setCurrentScene(scene* scn) { _currentScene.reset(scn); }

    protected:
        void processScene(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd);
        void updateFrameUniformBuffer(const camera* cmr, float aspectRatio);
        void updateLightUniformBuffer(const directional_light* dir_light);
        void updateObjectUniformBuffer(const render_object* obj);

        void initDefaultResources();

    private:
        std::shared_ptr<dynamic_rhi> _dynamicRHI;

        // default ub & tex
        uniform_buffer_handle g_defaultViewBufferHandle{};
        uniform_buffer_handle g_defaultObjectBufferHandle{};
        uniform_buffer_handle g_defaultLightBufferHandle{};

        sampler_handle g_defaultSamplerHandle{};
        intrusive_ptr<texture> g_defaultWhiteTexture{};
        intrusive_ptr<texture> g_defaultBlackTexture{};

        intrusive_ptr<scene> _currentScene;
    };
}