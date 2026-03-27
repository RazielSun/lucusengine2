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

            const transform& getTransform() const { return _transform; }
            void setTransform(const transform& t) { _transform = t; }

            const glm::vec3& getPosition() const { return _transform.position; }
            void setPosition(const glm::vec3& position) { _transform.position = position; }

            const glm::quat& getRotation() const { return _transform.rotation; }
            void setRotation(const glm::quat& rotation) { _transform.rotation = rotation; }
            void setRotationEuler(const glm::vec3& eulerAngles) { _transform.rotation = glm::quat(eulerAngles); }

            const glm::vec3& getScale() const { return _transform.scale; }
            void setScale(const glm::vec3& scale) { _transform.scale = scale; }

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