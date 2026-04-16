#pragma once

#include "pch.hpp"

#include "core_types.hpp"
#include "math_types.hpp"

namespace lucus
{
    // 2 and 3 are special for understanding that these are different constants
    constexpr u32 g_framesInFlight{ 2 };
    constexpr u32 g_swapchainImageCount{ 3 };
    // TODO: Make this dynamic or configurable
    constexpr u32 g_maxObjectBufferCount{ 32 };
    constexpr u32 g_maxTexturesCount{ 32 };
    constexpr u32 g_maxSamplersCount{ 32 };

    enum class uniform_buffer_type : u8
    {
        FRAME = 0,
        OBJECT = 1,
    };

    enum class shader_binding : u8
    {
        FRAME = 0,
        OBJECT = 1,
        MATERIAL = 2,
        VERTEX = 3,
    };

    struct resource_handle
    {
        resource_handle() : index(0) {}
        explicit resource_handle(u32 idx) : index(idx) {}
        
        u32 get() const { return index; }
        u32 as_index() const { return index - 1; }
        bool is_valid() const { return index > 0; }

        void invalidate() { index = 0; }

    protected:
        u32 index;
    };

    #define CUSTOM_RESOURCE_HANDLE(T) \
        struct T : public resource_handle \
        { \
            T() : resource_handle() {} \
            explicit T(u32 idx) : resource_handle(idx) {} \
        };

    // TODO: remove this, change to resource_handle
    // struct hash_handle
    // {
    //     hash_handle() : index(0) {}
    //     explicit hash_handle(uint64_t idx) : index(idx) {}
        
    //     uint64_t get() const { return index; }
    //     bool is_valid() const { return index > 0; }

    //     void invalidate() { index = 0; }

    // protected:
    //     uint64_t index;
    // };

    CUSTOM_RESOURCE_HANDLE(window_handle);
    CUSTOM_RESOURCE_HANDLE(window_context_handle);
    CUSTOM_RESOURCE_HANDLE(mesh_handle);
    CUSTOM_RESOURCE_HANDLE(texture_handle);
    CUSTOM_RESOURCE_HANDLE(pipeline_state_handle);
    CUSTOM_RESOURCE_HANDLE(uniform_buffer_handle);

    struct frame_uniform_buffer
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct object_uniform_buffer
    {
        alignas(16) glm::mat4 model;
    };

    // struct render_instance
    // {
    //     render_object_handle object;
    //     object_uniform_buffer object_data;

    //     mesh_handle mesh;
    //     material_handle material;
    // };

    // // TODO: replace with gpu_command_buffer
    // struct command_buffer
    // {
    //     frame_uniform_buffer frame_ubo;
    //     std::vector<render_instance> render_list;
    // };

    struct vertex
    {
        glm::vec3 position{0.0f};
        glm::vec2 texCoords{0.0f};
        glm::vec3 color{1.0f};
    };

    struct gpu_mesh
    {
        u32 vertexCount{0};
        u32 indexCount{0};
    };
}