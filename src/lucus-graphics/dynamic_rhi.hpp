#pragma once

#include "pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class window;
    class material;

    std::shared_ptr<class dynamic_rhi> create_dynamic_rhi();

    class dynamic_rhi
    {
    public:
        dynamic_rhi() = default;
        virtual ~dynamic_rhi() = default;

        virtual void init() = 0;

        virtual viewport_handle createViewport(const window_handle& handle) = 0;

        virtual void beginFrame(const viewport_handle& viewport) = 0;
        virtual void endFrame() = 0;

        virtual void submit(const command_buffer& cmd) = 0;

        virtual material_handle createMaterial(material* mat) = 0;
    };
}