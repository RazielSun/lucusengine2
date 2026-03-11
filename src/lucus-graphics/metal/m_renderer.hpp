#pragma once

#include "m_pch.hpp"

#include "renderer.hpp"

namespace lucus
{
    class m_renderer : public renderer
    {
        public:
            m_renderer();
            ~m_renderer() override = default;

            virtual bool init() override;
            virtual bool prepare(std::shared_ptr<window> window) override;
            virtual void tick() override;
            virtual void cleanup() override;
    };
}