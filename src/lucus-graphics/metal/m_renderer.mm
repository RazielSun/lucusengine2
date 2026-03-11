#include "m_renderer.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

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
    initDevices();
    return true;
}

bool m_renderer::prepare(std::shared_ptr<window> window)
{
    createMetalLayerAndView(window);
    return true;
}

void m_renderer::tick()
{
}

void m_renderer::cleanup()
{
    _metalLayer = nil;
    _device.reset();
}

void m_renderer::initDevices()
{
    // application_info& app_info = application_info::instance();
    // engine_info& engine_info = engine_info::instance();

    _device = std::make_unique<m_device>();
    _device->createLogicalDevice();
}

void m_renderer::createMetalLayerAndView(std::shared_ptr<window> window)
{
    NSWindow* ns_window = glfwGetCocoaWindow(window->getGLFWwindow());
    if (!ns_window)
    {
        throw std::runtime_error("Failed to get Cocoa window from GLFW");
    }

    NSView* content_view = [ns_window contentView];
    if (!content_view)
    {
        throw std::runtime_error("Failed to get NSWindow content view");
    }

    _metalLayer = [CAMetalLayer layer];
    _metalLayer.device = _device->getDevice();
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.framebufferOnly = YES;
    _metalLayer.contentsScale = [ns_window backingScaleFactor];

    _metalLayer.drawableSize = CGSizeMake((CGFloat)window->framebuffer_width(), (CGFloat)window->framebuffer_height());

    [content_view setWantsLayer:YES];
    [content_view setLayer:_metalLayer];
}