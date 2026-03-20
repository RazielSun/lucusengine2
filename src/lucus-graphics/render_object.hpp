#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "mesh.hpp"
#include "material.hpp"

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

        private:
            intrusive_ptr<mesh> _mesh;
            intrusive_ptr<material> _material;
    };
}