#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

window::window(int width, int height, const std::string& title)
    : _width(width), _height(height), _title(title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(_width, _height, _title.c_str(), nullptr, nullptr);
    if (!_window)
    {
        const char* error_description = nullptr;
        const int error_code = glfwGetError(&error_description);
        std::string message = "Failed to create GLFW window (error " + std::to_string(error_code);
        if (error_description)
        {
            message += ": ";
            message += error_description;
        }
        message += ")";
        throw std::runtime_error(message);
    }

    glfwGetFramebufferSize(_window, &_framebufferWidth, &_framebufferHeight);

    std::printf("Created window with size %dx%d (framebuffer size %dx%d)\n", _width, _height, _framebufferWidth, _framebufferHeight);
}

window::~window()
{
    if (_window)
    {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }
}

void window::close()
{
    glfwSetWindowShouldClose(_window, GLFW_TRUE);
}

bool window::shouldClose() const
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
