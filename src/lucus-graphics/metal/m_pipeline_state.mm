#include "m_pipeline_state.hpp"

#include "filesystem.hpp"

using namespace lucus;

m_pipeline_state::m_pipeline_state(id<MTLDevice> device) : _device(device)
{
    //
}

m_pipeline_state::~m_pipeline_state()
{
    _device = nil;
    _pipeline = nil;
}

void m_pipeline_state::init(const std::string& shaderName, MTLPixelFormat colorFormat)
{
    id<MTLLibrary> library = loadLibrary(shaderName);

    id<MTLFunction> vs = [library newFunctionWithName:@"vsMain"];
    id<MTLFunction> fs = [library newFunctionWithName:@"fsMain"];
    if (!vs || !fs) {
        throw std::runtime_error("Failed to load shader functions");
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vs;
    desc.fragmentFunction = fs;
    desc.colorAttachments[0].pixelFormat = colorFormat;

    NSError* error = nil;
    id<MTLRenderPipelineState> pso =
        [_device newRenderPipelineStateWithDescriptor:desc error:&error];

    if (!pso) {
        std::string msg = "Failed to create pipeline state: ";
        if (error) {
            msg += [[error localizedDescription] UTF8String];
        }
        throw std::runtime_error(msg);
    }

    _pipeline = pso;
}

// void m_renderer::createPipeline()
// {
//     id<MTLLibrary> library = loadLibrary("shaders/triangle.metal.metallib");
//     _pipeline = createPipeline(library, _metalLayer.pixelFormat);
// }

id<MTLLibrary> m_pipeline_state::loadLibrary(const std::string& filename) const
{
    // auto shaderCode = filesystem::instance().read_shader(filepath);

    std::string path = filesystem::instance().get_shader(filename + ".metallib");

    NSError* error = nil;
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];
    id<MTLLibrary> lib = [_device newLibraryWithURL:url error:&error];
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