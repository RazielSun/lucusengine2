#pragma once

#include "pch.hpp"

#include "renderer.hpp"

namespace lucus
{
    class dx_renderer : public renderer
    {
        public:
            dx_renderer();
            ~dx_renderer() override = default;

            virtual bool init() override;
            virtual bool prepare(std::shared_ptr<window> window) override;
            virtual void tick() override;
            virtual void cleanup() override;
    };
}