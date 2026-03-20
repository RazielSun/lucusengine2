#pragma once

#include "pch.hpp"

namespace lucus
{
    void bind_graphics_module();

    void bind_mesh_class();
    void bind_material_class();
    void bind_render_object_class();

    void bind_window_manager_class_and_object();
    void bind_renderer_class_and_object();
}