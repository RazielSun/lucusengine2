#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "render_types.hpp"
#include "intrusive_ptr.hpp"

#include "render_object.hpp"
#include "camera.hpp"

namespace lucus
{
    class dynamic_rhi;

    class renderer : public singleton<renderer>
    {
    public:
        void init(window_handle handle);

        void tick(float dt);
        void cleanup();

        render_object* emplaceRenderObject();

        void setCamera(camera* cam) { _camera.reset(cam); }
    
    protected:
        void updateFrameUniformBuffer(const window_context_handle& ctx_handle, frame_uniform_buffer& ubo);
        void processObjects(command_buffer& cmd);

    private:
        std::shared_ptr<dynamic_rhi> _dynamicRHI;

        std::vector<intrusive_ptr<render_object>> _renderObjects;

        intrusive_ptr<camera> _camera;
    };
}