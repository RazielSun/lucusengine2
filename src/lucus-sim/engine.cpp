#include "engine.hpp"

#include "bind_core.hpp"
#include "bind_graphics.hpp"

#include "commandline_args.hpp"
#include "filesystem.hpp"
#include "script_manager.hpp"
#include "window_manager.hpp"

#include "renderer.hpp"

using namespace lucus;

void engine::run(int argc, char** argv)
{
    commandline_args::instance().init(argc, argv);

    script_manager::instance().init();

    bind_core_module();
    bind_graphics_module();

    if (!script_manager::instance().build_module("main.as", "Entry")) {
        throw std::runtime_error("Failed to build main.as script");
    }

    if (!script_manager::instance().run_func("Entry", "void main()")) {
        throw std::runtime_error("Failed to run main() from Entry module");
    }

    auto handle = window_manager::instance().getMainWindow();
    if (!handle.is_valid()) {
        throw std::runtime_error("Failed to get main window handle");
    }

    auto mainWindow = window_manager::instance().getWindow(handle);
    while (mainWindow && !mainWindow->shouldClose())
    {
        mainWindow->tick();

        renderer::instance().tick(0.f);
    }
    
    renderer::instance().cleanup();
}