#pragma once

#include "pch.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace lucus
{
    struct transform
    {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
    };

    namespace math
    {
        glm::mat4 transform_to_mat4(const transform& t);
    }
}