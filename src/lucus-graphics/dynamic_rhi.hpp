#pragma once

#include "pch.hpp"

#include "core_types.hpp"
#include "render_types.hpp"

namespace lucus
{
    class window;
    class material;
    class mesh;
    class texture;
    class render_object;
    struct gpu_command_buffer;

    std::shared_ptr<class dynamic_rhi> create_dynamic_rhi();

    class dynamic_rhi
    {
    public:
        dynamic_rhi() = default;
        virtual ~dynamic_rhi() = default;

        virtual void init() = 0;

        virtual window_context_handle createWindowContext(const window_handle& handle) = 0;
        virtual const std::vector<window_context_handle>& getWindowContexts() const = 0;
        virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const = 0;

        virtual pipeline_state_handle createPSO(material* mat) = 0;
        virtual mesh_handle createMesh(mesh* msh) = 0;
        virtual texture_handle loadTexture(texture* tex) = 0;

        virtual uniform_buffer_handle createUniformBuffer(uniform_buffer_type ub_type, size_t bufferSize) = 0;
        virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) = 0;

        virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd) = 0;
    };
}