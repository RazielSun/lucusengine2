#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

int window::g_windows_count = 0;

window::window(int width, int height, const std::string& title)
    : _width(width), _height(height), _title(title)
{
    if (g_windows_count == 0)
    {
        if (glfwInit() != GLFW_TRUE)
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(_width, _height, _title.c_str(), nullptr, nullptr);
    if (!_window)
    {
        if (g_windows_count == 0)
        {
            glfwTerminate();
        }
        throw std::runtime_error("Failed to create GLFW window");
    }

    ++g_windows_count;

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

    if (g_windows_count > 0)
    {
        --g_windows_count;
        if (g_windows_count == 0)
        {
            glfwTerminate();
        }
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
