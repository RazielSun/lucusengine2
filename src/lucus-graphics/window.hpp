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

        int width() const;
        int height() const;

        int framebuffer_width() const;
        int framebuffer_height() const;
        
        bool shouldClose();
        void tick();

        GLFWwindow* getGLFWwindow();
    
    protected:
        void create();
        void destroy();

    private:
        GLFWwindow* _window;

        int _width;
        int _height;

        int _framebufferWidth;
        int _framebufferHeight;
    };
} // namespace lucus