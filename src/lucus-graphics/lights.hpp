#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "math_types.hpp"
#include "render_types.hpp"

namespace lucus
{
    class directional_light : public object
    {
        friend class renderer;
        
    public:
        static directional_light* create_factory() { return new directional_light(); }

        void setPosition(const glm::vec3& pos);
        glm::vec3 getPosition() const { return _transform.position; }

        void setRotation(const glm::quat& rot);
        glm::quat getRotation() const { return _transform.rotation; }
        void setRotationEuler(const glm::vec3& eulerAngles);

        glm::vec3 getDirection() const;

        void setOrthoSize(float orthoSize) { _orthoSize = orthoSize; }
        void setZNear(float zNear) { _zNear = zNear; }
        void setZFar(float zFar) { _zFar = zFar; }

        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;
    
    protected:
        const uniform_buffer_handle& getHandle() const { return _handle; }
        void setHandle(const uniform_buffer_handle& handle) { _handle = handle; }

    private:
        transform _transform;

        float _orthoSize = 50.0f;
        float _zNear = 0.1f;
        float _zFar = 500.0f;

        mutable bool bViewMatrixDirty = true;
        mutable glm::mat4 cachedViewMatrix;

        // transient
        uniform_buffer_handle _handle;
    };
}
