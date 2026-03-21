#import "m_viewport.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void m_viewport::init(id<MTLDevice> device, window* window)
{
    assert(device);
    assert(window);

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
    _metalLayer.device = device;
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.framebufferOnly = YES;
    _metalLayer.maximumDrawableCount = g_swapchainImageCount;
    _metalLayer.contentsScale = [ns_window backingScaleFactor];

    _metalLayer.drawableSize = CGSizeMake((CGFloat)window->framebuffer_width(), (CGFloat)window->framebuffer_height());

    [content_view setWantsLayer:YES];
    [content_view setLayer:_metalLayer];
}

id<CAMetalDrawable> m_viewport::getNewDrawable()
{
    return [_metalLayer nextDrawable];
}

