#pragma once

#include "pch.hpp"

namespace lucus
{
    class mesh;
    class scene;

    mesh* load_mesh_gltf(const std::string& fileName);
    scene* load_scene_gltf(const std::string& fileName);
}