#include "math_types.hpp"

using namespace lucus;

glm::mat4 lucus::math::transform_to_mat4(const transform& t)
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), t.position);
    glm::mat4 R = glm::mat4_cast(t.rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), t.scale);
    return T * R * S;
}