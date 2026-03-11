#include "m_renderer.hpp"

std::shared_ptr<lucus::renderer> lucus::create_renderer()
{
    auto renderer = std::make_shared<lucus::m_renderer>();
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
}

using namespace lucus;

m_renderer::m_renderer()
{
}

bool m_renderer::init()
{
    return true;
}

bool m_renderer::prepare(std::shared_ptr<window> window)
{
    return true;
}

void m_renderer::tick()
{
}

void m_renderer::cleanup()
{
}