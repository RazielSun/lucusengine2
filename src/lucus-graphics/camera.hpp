#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "math_types.hpp"

namespace lucus
{
    class camera : public object
    {
        public:
            static camera* create_factory() { return new camera(); }

            void setPosition(const glm::vec3& pos) { _transform.position = pos; }
            glm::vec3 getPosition() const { return _transform.position; }

            void setPositionXYZ(float x, float y, float z) { _transform.position = glm::vec3(x, y, z); }

            glm::mat4 getViewMatrix() const;
            glm::mat4 getProjectionMatrix() const;

        private:
            transform _transform;

            float _fovYRadians = glm::radians(60.0f);
            float _aspect = 16.0f / 9.0f;
            float _zNear = 0.1f;
            float _zFar = 1000.0f;
    };
}