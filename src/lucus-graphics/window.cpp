#include "window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using namespace lucus;

window::window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(WIDTH, HEIGHT, "Lucus Engine", nullptr, nullptr);
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