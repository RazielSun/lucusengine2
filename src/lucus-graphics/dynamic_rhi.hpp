#pragma once

#include "pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class window;
    class material;
    class mesh;
    class render_object;

    std::shared_ptr<class dynamic_rhi> create_dynamic_rhi();

    class dynamic_rhi
    {
    public:
        dynamic_rhi() = default;
        virtual ~dynamic_rhi() = default;

        virtual void init() = 0;

        virtual window_context_handle createWindowContext(const window_handle& handle) = 0;
        virtual const std::vector<window_context_handle>& getWindowContexts() const = 0;
        virtual float getWindowContextAspectRatio(const window_context_handle& handle) const = 0;

        virtual void beginFrame(const window_context_handle& handle) = 0;
        virtual void submit(const window_context_handle& handle, const command_buffer& cmd) = 0;
        virtual void endFrame(const window_context_handle& handle) = 0;

        virtual material_handle createMaterial(material* mat) = 0;
        // virtual mesh_handle createMesh(mesh* msh) = 0;
        virtual render_object_handle createUniformBuffer(render_object* obj) = 0;
    };
}