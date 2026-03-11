#include "window.hpp"

#include "application_info.hpp"

#include "glfw_include.hpp"

using namespace lucus;

window::window()
{
    application_info& app_info = application_info::instance();

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _width = app_info.width();
    _height = app_info.height();
    _window = glfwCreateWindow(_width, _height, app_info.app_name().c_str(), nullptr, nullptr);

    glfwGetFramebufferSize(_window, &_framebufferWidth, &_framebufferHeight);

    std::printf("Created window with size %dx%d (framebuffer size %dx%d)\n", _width, _height, _framebufferWidth, _framebufferHeight);
}

window::~window()
{
    glfwDestroyWindow(_window);
    glfwTerminate();
}

int window::width() const
{
    return _width;
}

int window::height() const
{
    return _height;
}

int window::framebuffer_width() const
{
    return _framebufferWidth;
}

int window::framebuffer_height() const
{
    return _framebufferHeight;
}

bool window::shouldClose()
{
    return glfwWindowShouldClose(_window);
}

void window::tick()
{
    glfwPollEvents();
}

GLFWwindow* window::getGLFWwindow()
{
    return _window;
}