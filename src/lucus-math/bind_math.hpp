#pragma once

#include "pch.hpp"

namespace lucus
{
    void bind_math_module();

    namespace binds
    {
        constexpr const char* vec3_class_name = "Vec3";
    }
}