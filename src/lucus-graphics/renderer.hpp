#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "render_types.hpp"
#include "render_object.hpp"

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
    
    protected:
        void processObjects(command_buffer& cmd);

    private:
        std::shared_ptr<dynamic_rhi> _dynamicRHI;

        std::vector<intrusive_ptr<render_object>> _renderObjects;
        
        viewport_handle _mainViewport;
    };
}