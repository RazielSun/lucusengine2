#pragma once

#include "pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class mesh;
    class material;
    class scene;

    struct stbi_deleter {
        void operator()(u8* p) const;
    };
    using stbi_image_ptr = std::unique_ptr<u8, stbi_deleter>;

    mesh* load_mesh_gltf(const std::string& fileName);
    scene* load_scene_gltf(const std::string& fileName);
    scene* load_scene_with_material_gltf(const std::string& fileName, material* override_mat);

    stbi_image_ptr load_texture(const std::string& fileName, int& texWidth, int& texHeight, int& texChannels, texture_format format = texture_format::RGB_ALPHA);
}