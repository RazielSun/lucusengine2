#pragma once

#include "pch.hpp"

namespace lucus
{
    class window;
    class renderer;

    std::shared_ptr<renderer> create_renderer(std::shared_ptr<window> window);

    class renderer
    {
    public:
        virtual ~renderer() = default;

        virtual bool init(std::shared_ptr<window> window) = 0;
        virtual void tick() = 0;
        virtual void cleanup() = 0;
    };
}