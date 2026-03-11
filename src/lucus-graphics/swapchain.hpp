#pragma once

#include "pch.hpp"

namespace lucus
{
    class window;

    // interface
    class iswapchain
    {
        public:
            virtual ~iswapchain() = default;

            virtual void createSurface(std::shared_ptr<window> window) = 0;
            virtual void create(std::shared_ptr<window> window) = 0;
    };
}