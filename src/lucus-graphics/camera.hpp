#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "math_types.hpp"
#include "render_types.hpp"

namespace lucus
{
    class camera : public object
    {
        friend class renderer;
        
    public:
        static camera* create_factory() { return new camera(); }

        void setPosition(const glm::vec3& pos);
        glm::vec3 getPosition() const { return _transform.position; }

        void setRotation(const glm::quat& rot);
        glm::quat getRotation() const { return _transform.rotation; }
        void setRotationEuler(const glm::vec3& eulerAngles);
        void setFovYRadians(float fovYRadians) { _fovYRadians = fovYRadians; }
        void setZNear(float zNear) { _zNear = zNear; }
        void setZFar(float zFar) { _zFar = zFar; }

        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix(float aspectRatio) const;
    
    protected:
        const uniform_buffer_handle& getHandle() const { return _handle; }
        void setHandle(const uniform_buffer_handle& handle) { _handle = handle; }

    private:
        transform _transform;

        float _fovYRadians = glm::radians(60.0f);
        float _zNear = 0.1f;
        float _zFar = 1000.0f;

        mutable bool bCameraViewDirty = true;
        mutable glm::mat4 cachedViewMatrix;

        // transient
        uniform_buffer_handle _handle;
    };
}
