#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "window.hpp"
#include "render_types.hpp"

namespace lucus
{
    class window_manager : public singleton<window_manager>
    {
    public:
        void createWindow(int width, int height, const std::string& title);

        window_handle getMainWindow() const;
        window* getWindow(const window_handle& handle) const;

    protected:
        //

    private:
        window_handle _mainWindow;
        std::vector<std::unique_ptr<window>> _windows;
    };
}