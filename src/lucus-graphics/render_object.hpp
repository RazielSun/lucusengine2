#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "mesh.hpp"
#include "material.hpp"
#include "render_types.hpp"
#include "math_types.hpp"

namespace lucus
{
    class render_object : public object
    {
        friend class renderer;

        public:
            void setMesh(mesh* mesh) { _mesh.reset(mesh); }
            mesh* getMesh() const { return _mesh.get(); }

            void setMaterial(material* material) { _material.reset(material); }
            material* getMaterial() const { return _material.get(); }

            const render_object_handle& getHandle() const { return _handle; }
            void setHandle(const render_object_handle& handle) { _handle = handle; }

        private:
            intrusive_ptr<mesh> _mesh;
            intrusive_ptr<material> _material;

            transform _transform;

            // transient
            render_object_handle _handle;
    };
}