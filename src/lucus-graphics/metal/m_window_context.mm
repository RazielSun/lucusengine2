#import "m_window_context.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void m_window_context::init(id<MTLDevice> device, window* window)
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

    metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.framebufferOnly = YES;
    metalLayer.maximumDrawableCount = g_swapchainImageCount;
    metalLayer.contentsScale = [ns_window backingScaleFactor];

    cachedDrawableSize = CGSizeMake((CGFloat)window->framebuffer_width(), (CGFloat)window->framebuffer_height());
    metalLayer.drawableSize = cachedDrawableSize;

    [content_view setWantsLayer:YES];
    [content_view setLayer:metalLayer];

    // Depth
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:depthFormat
                                                      width:(NSUInteger)cachedDrawableSize.width
                                                     height:(NSUInteger)cachedDrawableSize.height
                                                  mipmapped:NO];
    desc.storageMode = MTLStorageModePrivate;
    desc.usage = MTLTextureUsageRenderTarget;
    for (auto& depthTexture : depthTextures)
    {
        depthTexture = [device newTextureWithDescriptor:desc];
    }

    // Create a semaphore for synchronizing frame rendering
    frameSemaphore = dispatch_semaphore_create(g_framesInFlight);

    uniformbuffers.init(device, sizeof(frame_uniform_buffer) * g_framesInFlight);
}

void m_window_context::wait_frame()
{
    dispatch_semaphore_wait(frameSemaphore, DISPATCH_TIME_FOREVER);

    currentDrawable = getNewDrawable();
}

id<CAMetalDrawable> m_window_context::getNewDrawable()
{
    return [metalLayer nextDrawable];
}

CGSize m_window_context::getDrawableSize() const
{
    return metalLayer.drawableSize;
}
