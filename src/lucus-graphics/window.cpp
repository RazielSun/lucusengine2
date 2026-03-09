#include "window.hpp"

#include "application.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using namespace lucus;

window::window(const application_info* app_info)
{
    if (app_info == nullptr) {
        throw std::runtime_error("app_info cannot be null");
    }

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(app_info->width(), app_info->height(), app_info->app_name().c_str(), nullptr, nullptr);
}

window::~window()
{
    glfwDestroyWindow(_window);
    glfwTerminate();
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