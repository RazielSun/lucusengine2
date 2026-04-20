#pragma once

#include "pch.hpp"

#include "intrusive_ptr.hpp"
#include "render_object.hpp"
#include "camera.hpp"
#include "lights.hpp"

namespace lucus
{
    class scene : public object
    {
    public:
        static scene* create_factory();

        render_object* newObject();
        
        const camera* getCamera() const { return _camera.get(); }
        void setCamera(camera* cam) { _camera.reset(cam); }

        const directional_light* getDirectionalLight() const { return _dir_light.get(); }
        void setDirectionalLight(directional_light* light) { _dir_light.reset(light); }

        std::vector<intrusive_ptr<render_object>>& objects() { return _objects; }
        const std::vector<intrusive_ptr<render_object>>& objects() const { return _objects; }

    private:
        std::vector<intrusive_ptr<render_object>> _objects;

        intrusive_ptr<camera> _camera;
        intrusive_ptr<directional_light> _dir_light;
    };
}