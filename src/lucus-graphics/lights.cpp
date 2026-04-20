#include "lights.hpp"

using namespace lucus;

void directional_light::setPosition(const glm::vec3& pos)
{
    _transform.position = pos;
    bViewMatrixDirty = true;
}

void directional_light::setRotation(const glm::quat& rot)
{
    _transform.rotation = rot;
    bViewMatrixDirty = true;
}

void directional_light::setRotationEuler(const glm::vec3& eulerAngles)
{
    _transform.rotation = glm::quat(glm::radians(eulerAngles));
    bViewMatrixDirty = true;
}

glm::vec3 directional_light::getDirection() const
{
    return glm::normalize(_transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::mat4 directional_light::getViewMatrix() const
{
    if (bViewMatrixDirty)
    {
        const glm::mat4 inverseRotationMatrix = glm::mat4_cast(glm::conjugate(_transform.rotation));
        const glm::vec3 translatedPosition = inverseRotationMatrix * glm::vec4(-_transform.position, 1.0f);

        cachedViewMatrix = inverseRotationMatrix;
        cachedViewMatrix[3] = glm::vec4(translatedPosition, 1.0f);
        bViewMatrixDirty = false;
    }
    return cachedViewMatrix;
}

glm::mat4 directional_light::getProjectionMatrix() const
{
    return glm::ortho(-_orthoSize, _orthoSize, -_orthoSize, _orthoSize, _zNear, _zFar);
}
