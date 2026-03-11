#include "dx_renderer.hpp"

std::shared_ptr<lucus::renderer> lucus::create_renderer()
{
    auto renderer = std::make_shared<lucus::dx_renderer>();
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
}

using namespace lucus;

dx_renderer::dx_renderer()
{
}

bool dx_renderer::init()
{
    return true;
}

bool dx_renderer::prepare(std::shared_ptr<window> window)
{
    return true;
}

void dx_renderer::tick()
{
}

void dx_renderer::cleanup()
{
}