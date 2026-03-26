#include "camera.hpp"

using namespace lucus;

glm::mat4 camera::getViewMatrix() const
{
    // For simplicity, we assume the camera is always looking at the negative Z direction and has no rotation.
    // In a real implementation, you would likely want to support arbitrary orientations and look-at targets.
    return glm::lookAt(_transform.position, _transform.position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 camera::getProjectionMatrix() const
{
    return glm::perspective(_fovYRadians, _aspect, _zNear, _zFar);
}