#include "engine.hpp"

#include "commandline_args.hpp"
#include "filesystem.hpp"
#include "script_manager.hpp"

#include "window.hpp"
#include "renderer.hpp"

using namespace lucus;

void engine::run(int argc, char** argv)
{
    commandline_args::instance().init(argc, argv);

    script_manager::instance().init();

    _app_info = application_info();
    bind_application_info();
    bind_application_info_object(_app_info, "app_info");

    if (!script_manager::instance().run_script("main.as")) {
        throw std::runtime_error("Failed to run main.as script");
    }

    _window = std::make_shared<window>(&_app_info);
    _renderer = create_renderer(_window);

    while (_window->shouldClose() == false)
    {
        _window->tick();
        _renderer->tick();
    }
    
    _renderer->cleanup();
    _renderer.reset();
    _window.reset();
}