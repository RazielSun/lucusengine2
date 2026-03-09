#pragma once

#include "pch.hpp"

class GLFWwindow;

namespace lucus
{
    class application_info;

    class window
    {
    public:
        window(const application_info* app_info);
        ~window();
        
        bool shouldClose();
        void tick();

        GLFWwindow* getGLFWwindow();
    
    protected:
        void create();
        void destroy();

    private:
        GLFWwindow* _window;
    };
} // namespace lucus