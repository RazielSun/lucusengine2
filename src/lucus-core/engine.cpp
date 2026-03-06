#include "engine.hpp"

#include "window.hpp"
#include "renderer.hpp"

using namespace lucus;

void engine::run()
{
    _window = std::make_shared<window>();

    _renderer = std::make_shared<renderer>();
    _renderer->init(_window);

    while (_window->shouldClose() == false)
    {
        _window->tick();
        _renderer->mainLoop();
    }
    
    _renderer->cleanup();

    _renderer.reset();
    _window.reset();
}