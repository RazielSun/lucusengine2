#pragma once

#include "pch.hpp"

class GLFWwindow;

namespace lucus
{
    class window
    {
    public:
        window(int width, int height, const std::string& title);
        ~window();

        int width() const { return _width; }
        int height() const { return _height; }

        const std::string& title() const { return _title; }

        int framebuffer_width() const { return _framebufferWidth; }
        int framebuffer_height() const { return _framebufferHeight; }
        
        void close();
        bool shouldClose() const;
        void tick();

        GLFWwindow* getGLFWwindow();
    
    protected:
        void create();
        void destroy();

    private:
        GLFWwindow* _window;

        std::string _title;

        int _width;
        int _height;

        int _framebufferWidth;
        int _framebufferHeight;
    };
} // namespace lucus