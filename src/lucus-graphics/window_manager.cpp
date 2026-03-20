#include "window_manager.hpp"

#include "script_manager.hpp"
#include "renderer.hpp"

using namespace lucus;

void window_manager::createWindow(int width, int height, const std::string& title)
{
    auto new_window = std::make_unique<window>(width, height, title);

    auto new_window_handle = window_handle(static_cast<int>(_windows.size()));
    if (!_mainWindow.is_valid()) {
        _mainWindow = new_window_handle;
    }
    _windows.push_back(std::move(new_window));

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
    int index = handle.get();
    return index < _windows.size() ? _windows[index].get() : nullptr;
}