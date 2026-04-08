#include "camera.hpp"

using namespace lucus;

void camera::setPosition(const glm::vec3& pos)
{
    _transform.position = pos;
    bCameraViewDirty = true;
}

void camera::setRotation(const glm::quat& rot)
{
    _transform.rotation = rot;
    bCameraViewDirty = true;
}

void camera::setRotationEuler(const glm::vec3& eulerAngles)
{
    _transform.rotation = glm::quat(eulerAngles);
    bCameraViewDirty = true;
}

glm::mat4 camera::getViewMatrix() const
{
    if (bCameraViewDirty)
    {
        const glm::mat4 inverseRotationMatrix = glm::mat4_cast(glm::conjugate(_transform.rotation));
        const glm::vec3 translatedPosition = inverseRotationMatrix * glm::vec4(-_transform.position, 1.0f);

        cachedViewMatrix = inverseRotationMatrix;
        cachedViewMatrix[3] = glm::vec4(translatedPosition, 1.0f);
        bCameraViewDirty = false;
    }
    return cachedViewMatrix;
}

glm::mat4 camera::getProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(_fovYRadians, aspectRatio, _zNear, _zFar);
}
