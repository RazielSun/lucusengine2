#include "m_renderer.hpp"

#include "filesystem.hpp"
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
    createPipeline();
    return true;
}

void m_renderer::tick()
{
    prepareFrame();
    buildCommandBuffer();
    submitFrame();
}

void m_renderer::cleanup()
{
    _pipeline = nil;
    _metalLayer = nil;
    deviceMTL = nil;
    _device.reset();
}

void m_renderer::initDevices()
{
    // application_info& app_info = application_info::instance();
    // engine_info& engine_info = engine_info::instance();

    _device = std::make_unique<m_device>();
    _device->createLogicalDevice();

    deviceMTL = _device->getDevice();
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
    _metalLayer.device = deviceMTL;
    _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalLayer.framebufferOnly = YES;
    _metalLayer.contentsScale = [ns_window backingScaleFactor];

    _metalLayer.drawableSize = CGSizeMake((CGFloat)window->framebuffer_width(), (CGFloat)window->framebuffer_height());

    [content_view setWantsLayer:YES];
    [content_view setLayer:_metalLayer];
}

void m_renderer::createPipeline()
{
    id<MTLLibrary> library = loadLibrary("shaders/triangle.metal.metallib");
    _pipeline = createPipeline(library, _metalLayer.pixelFormat);
}

id<MTLLibrary> m_renderer::loadLibrary(const std::string& filename)
{
    std::string path = filesystem::instance().get_path(filename);

    NSError* error = nil;
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];
    id<MTLLibrary> lib = [deviceMTL newLibraryWithURL:url error:&error];
    if (!lib)
    {
        std::string msg = "Failed to load metallib: ";
        if (error) {
            msg += [[error localizedDescription] UTF8String];
        }
        throw std::runtime_error(msg);
    }
    return lib;
}

id<MTLRenderPipelineState> m_renderer::createPipeline(id<MTLLibrary> library, MTLPixelFormat colorFormat)
{
    id<MTLFunction> vs = [library newFunctionWithName:@"vs_main"];
    id<MTLFunction> fs = [library newFunctionWithName:@"fs_main"];
    if (!vs || !fs) {
        throw std::runtime_error("Failed to load shader functions");
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vs;
    desc.fragmentFunction = fs;
    desc.colorAttachments[0].pixelFormat = colorFormat;

    NSError* error = nil;
    id<MTLRenderPipelineState> pso =
        [deviceMTL newRenderPipelineStateWithDescriptor:desc error:&error];

    if (!pso) {
        std::string msg = "Failed to create pipeline state: ";
        if (error) {
            msg += [[error localizedDescription] UTF8String];
        }
        throw std::runtime_error(msg);
    }

    return pso;
}

void m_renderer::prepareFrame()
{
    _currentDrawable = [_metalLayer nextDrawable];
}

void m_renderer::buildCommandBuffer()
{
    if (!_currentDrawable)
    {
        return;
    }

    _currentBuffer = [_device->getCommandPool() commandBuffer];

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = _currentDrawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    id<MTLRenderCommandEncoder> enc = [_currentBuffer renderCommandEncoderWithDescriptor:pass];

    [enc setRenderPipelineState:_pipeline];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:0
            vertexCount:3];
    [enc endEncoding];
}

void m_renderer::submitFrame()
{
    if (!_currentDrawable || !_currentBuffer)
    {
        return;
    }

    [_currentBuffer presentDrawable:_currentDrawable];
    [_currentBuffer commit];
}