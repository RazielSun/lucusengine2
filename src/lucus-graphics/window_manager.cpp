#include "window_manager.hpp"

#include "script_manager.hpp"
#include "renderer.hpp"
#include "glfw_include.hpp"

using namespace lucus;

window_manager::window_manager()
{
    if (glfwInit() != GLFW_TRUE)
    {
        const char* error_description = nullptr;
        const int error_code = glfwGetError(&error_description);
        std::string message = "Failed to initialize GLFW (error " + std::to_string(error_code);
        if (error_description)
        {
            message += ": ";
            message += error_description;
        }
        message += ")";
        throw std::runtime_error(message);
    }
}

window_manager::~window_manager()
{
    _windows.clear();
    _mainWindow.invalidate();
    glfwTerminate();
}

void window_manager::createWindow(int width, int height, const std::string& title)
{
    auto new_window = std::make_unique<window>(width, height, title);

    _windows.push_back(std::move(new_window));

    auto new_window_handle = window_handle(static_cast<int>(_windows.size()));
    if (!_mainWindow.is_valid()) {
        _mainWindow = new_window_handle;
    }

    // TODO: this coupling is not ideal, but it allows us to initialize the renderer with the main window's handle
    renderer::instance().init(new_window_handle);
}

window_handle window_manager::getMainWindow() const
{
    return _mainWindow;
}

window* window_manager::getWindow(const window_handle& handle) const
{
    if (!handle.is_valid()) {
        return nullptr;
    }
    int index = handle.as_index(); // Convert to 0-based index
    return index < _windows.size() ? _windows[index].get() : nullptr;
}