#pragma once

#include "pch.hpp"

#include "application.hpp"

namespace lucus
{
    class engine
    {
    public:
        void run(int argc, char** argv);

    private:
        application_info _app_info;

        std::shared_ptr<class window> _window;
        std::shared_ptr<class renderer> _renderer;
    };
} // namespace lucus