#pragma once

#include "pch.hpp"

#include "math_types.hpp"

namespace lucus
{
    constexpr uint32_t g_framesInFlight{ 2 };
    constexpr uint32_t g_swapchainImageCount{ 3 };
    constexpr uint32_t g_maxObjectBufferCount{ 32 }; // TODO: Make this dynamic or configurable

    struct resource_handle
    {
        resource_handle() : index(0) {}
        explicit resource_handle(uint32_t idx) : index(idx) {}
        
        uint32_t get() const { return index; }
        uint32_t as_index() const { return index - 1; }
        bool is_valid() const { return index > 0; }

        void invalidate() { index = 0; }

    protected:
        uint32_t index;
    };

    struct hash_handle
    {
        hash_handle() : index(0) {}
        explicit hash_handle(uint64_t idx) : index(idx) {}
        
        uint64_t get() const { return index; }
        bool is_valid() const { return index > 0; }

        void invalidate() { index = 0; }

    protected:
        uint64_t index;
    };

    struct window_handle : public resource_handle
    {
        window_handle() : resource_handle() {}
        explicit window_handle(int idx) : resource_handle(idx) {}
    };

    struct window_context_handle : public resource_handle
    {
        window_context_handle() : resource_handle() {}
        explicit window_context_handle(int idx) : resource_handle(idx) {}
    };

    struct material_handle : public hash_handle
    {
        material_handle() : hash_handle() {}
        explicit material_handle(uint64_t idx) : hash_handle(idx) {}
    };

    struct mesh_handle : public hash_handle
    {
        mesh_handle() : hash_handle() {}
        explicit mesh_handle(uint64_t idx) : hash_handle(idx) {}
    };

    struct render_object_handle : public resource_handle
    {
        render_object_handle() : resource_handle() {}
        explicit render_object_handle(int idx) : resource_handle(idx) {}
    };

    struct frame_uniform_buffer
    {
        // alignas(16) glm::mat4 model; // TEST ONLY
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct object_uniform_buffer
    {
        alignas(16) glm::mat4 model;
    };

    struct render_instance
    {
        render_object_handle object;
        object_uniform_buffer object_data;

        mesh_handle mesh;
        material_handle material;
    };

    struct command_buffer
    {
        frame_uniform_buffer frame_ubo;
        std::vector<render_instance> render_list;
    };

    struct vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 color{1.0f};
    };
}