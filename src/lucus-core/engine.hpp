#pragma once

#include "pch.hpp"

namespace lucus
{
    class engine
    {
    public:
        void run();

    private:
        std::shared_ptr<class window> _window;
        std::shared_ptr<class renderer> _renderer;
    };
} // namespace lucus