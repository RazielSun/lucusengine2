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
    constexpr u32 g_maxUniformBufferCount{ 32 };
    constexpr u32 g_maxTexturesCount{ 32 };
    constexpr u32 g_maxSamplersCount{ 32 };
    constexpr u32 g_maxMrtColorTargets{ 3 };

    enum class render_mode : u8
    {
        FORWARD = 0,
        DEFERRED = 1,
    };

    enum class render_pass_name : u8
    {
        SHADOW_PASS = 0,
        FORWARD_PASS = 1,
        GBUFFER_PASS = 2,
        DEFERRED_LIGHTING_PASS = 3,
    };

    // STRICT SHADER BIND CONTRACT
    enum class shader_binding : u8
    {
        VIEW                = 0,
        OBJECT              = 1,
        LIGHT               = 2,
        TEXTURE             = 3,
        SHADOW_MAP          = 4,
        SAMPLER             = 5,
        SHADOW_MAP_SAMPLER  = 6,
        GBUFFER_A           = 7, // NORMAL + Flags
        GBUFFER_B           = 8, // Metalness + Roughness + Specular + ShadingModel
        GBUFFER_C           = 9, // ALBEDO + AO
        GBUFFER_DEPTH       = 10,
        GBUFFER_SAMPLER     = 11,
    };

    enum class shader_binding_stage : u8
    {
        VERTEX      = 0,
        FRAGMENT    = 1,
        BOTH        = 2,
    };

    // STBI
    enum class texture_format : u8
    {
        GREY       = 1,
        GREY_ALPHA = 2,
        RGB        = 3,
        RGB_ALPHA  = 4,
    };

    enum class render_target_type : u8
    {
        NONE = 0,
        COLOR = 1,
        DEPTH = 2,
    };

    enum class resource_state : u8
    {
        UNDEFINED,
        DEPTH_WRITE,
        SHADER_READ,
        COLOR_WRITE,
        PRESENT,
        COPY_SRC,
        COPY_DST,
        GBUFFER_WRITE,
    };

    enum class image_barrier_aspect : u8
    {
        NONE = 0,
        COLOR = 1,
        DEPTH = 2,
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

    CUSTOM_RESOURCE_HANDLE(window_handle);
    CUSTOM_RESOURCE_HANDLE(window_context_handle);
    CUSTOM_RESOURCE_HANDLE(mesh_handle);
    CUSTOM_RESOURCE_HANDLE(texture_handle);
    CUSTOM_RESOURCE_HANDLE(render_target_handle);
    CUSTOM_RESOURCE_HANDLE(sampler_handle);
    CUSTOM_RESOURCE_HANDLE(pipeline_state_handle);
    CUSTOM_RESOURCE_HANDLE(uniform_buffer_handle);

    struct frame_uniform_buffer
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 invViewProj;
        glm::vec4 position;
        glm::vec4 other; // xy - screen size
    };

    struct object_uniform_buffer
    {
        alignas(16) glm::mat4 model;
    };

    struct light_uniform_buffer
    {
        glm::vec4 position{0.0f};
        glm::vec4 direction{0.0f};
        glm::vec4 color{1.0f}; // xyz - color, w - intensity
        glm::vec4 ambient{1.0f}; // xyz - color, w - intensity
        alignas(16) glm::mat4 viewProj;
    };

    struct vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec2 texCoords{0.0f};
        glm::vec3 color{1.0f};
    };

    struct gpu_mesh
    {
        u32 vertexCount{0};
        u32 indexCount{0};
    };
}