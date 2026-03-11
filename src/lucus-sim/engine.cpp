#include "engine.hpp"

#include "application_info.hpp"
#include "engine_info.hpp"
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

    engine_info& engine_info = engine_info::instance();
    bind_engine_info();
    bind_engine_info_object(engine_info, "engine_info");

    application_info& app_info = application_info::instance();
    bind_application_info();
    bind_application_info_object(app_info, "app_info");

    if (!script_manager::instance().run_script("main.as")) {
        throw std::runtime_error("Failed to run main.as script");
    }

    _window = std::make_shared<window>();
    _renderer = create_renderer();

    _renderer->prepare(_window);
    while (_window->shouldClose() == false)
    {
        _window->tick();
        _renderer->tick();
    }
    
    _renderer->cleanup();
    _renderer.reset();
    _window.reset();
}