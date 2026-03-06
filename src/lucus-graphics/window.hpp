#pragma once

#include "pch.hpp"

class GLFWwindow;

namespace lucus
{
    class window
    {
    public:
        window();
        ~window();
        
        bool shouldClose();
        void tick();

        GLFWwindow* getGLFWwindow();
    
    protected:
        void create();
        void destroy();

    private:
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* _window;
    };
} // namespace lucus