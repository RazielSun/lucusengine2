#pragma once

#include "pch.hpp"

namespace lucus
{
    class engine
    {
    public:
        void run(int argc, char** argv);

    private:
        std::shared_ptr<class window> _window;
        std::shared_ptr<class renderer> _renderer;
    };
} // namespace lucus