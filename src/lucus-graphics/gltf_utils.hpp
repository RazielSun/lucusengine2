#pragma once

#include "pch.hpp"

namespace lucus
{
    class mesh;
    class scene;

    mesh* load_mesh_gltf(const std::string& fileName);
    scene* load_scene_gltf(const std::string& fileName);

    void* load_texture(const std::string& fileName, int& texWidth, int& texHeight, int& texChannels);
    void free_texture(void* texture_ptr);
}