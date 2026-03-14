#pragma once

#include "pch.hpp"

#include "render_config.hpp"

namespace lucus
{
    class window;
    class renderer;

    std::shared_ptr<renderer> create_renderer();

    class renderer
    {
    public:
        virtual ~renderer() = default;

        virtual bool init() = 0;
        virtual bool prepare(std::shared_ptr<window> window) = 0;
        virtual void tick() = 0;
        virtual void cleanup() = 0;
    };
}