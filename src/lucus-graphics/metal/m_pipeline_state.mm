#include "m_pipeline_state.hpp"

#include "filesystem.hpp"
#include "render_types.hpp"

using namespace lucus;

namespace
{
    std::string describe(NSError* error)
    {
        return error ? std::string([[error localizedDescription] UTF8String]) : std::string("no detail");
    }
}

void m_shader_library_cache::init(id<MTLDevice> device)
{
    assert(device);
    _device = device;
}

void m_shader_library_cache::cleanup()
{
    _libraries.clear();
    _device = nil;
}

id<MTLLibrary> m_shader_library_cache::get(const std::string& shaderName)
{
    auto found = _libraries.find(shaderName);
    if (found != _libraries.end())
    {
        return found->second;
    }

    id<MTLLibrary> library = compile(shaderName);
    _libraries.emplace(shaderName, library);
    return library;
}

id<MTLLibrary> m_shader_library_cache::compile(const std::string& shaderName) const
{
    const std::string fileName = shaderName + ".metal";
    const std::vector<char> source = filesystem::instance().read_shader(fileName);

    NSString* nsSource = [[NSString alloc] initWithBytes:source.data()
                                                  length:source.size()
                                                encoding:NSUTF8StringEncoding];
    if (!nsSource)
    {
        throw std::runtime_error("Failed to read Metal shader source: " + fileName);
    }

    MTLCompileOptions* options = [MTLCompileOptions new];

    NSError* error = nil;
    id<MTLLibrary> library = [_device newLibraryWithSource:nsSource options:options error:&error];
    if (!library)
    {
        throw std::runtime_error("Failed to compile Metal shader " + fileName + ": " + describe(error));
    }

    std::printf("Metal shader %s compiled successfully\n", fileName.c_str());
    return library;
}

m_pipeline_state::m_pipeline_state(id<MTLDevice> device) : _device(device)
{
}

void m_pipeline_state::init(id<MTLLibrary> library, const m_pipeline_state_desc& init_desc)
{
    assert(library);

    id<MTLFunction> vs = [library newFunctionWithName:@"vsMain"];
    id<MTLFunction> fs = [library newFunctionWithName:@"psMain"];
    if (!vs || !fs) {
        throw std::runtime_error("Failed to load shader functions");
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vs;
    desc.fragmentFunction = fs;
    desc.colorAttachments[0].pixelFormat = init_desc.colorFormat;
    desc.depthAttachmentPixelFormat = init_desc.depthFormat;

    desc.vertexDescriptor = init_desc.vertexDescriptor;

    NSError* error = nil;
    id<MTLRenderPipelineState> pso =
        [_device newRenderPipelineStateWithDescriptor:desc error:&error];

    if (!pso) {
        throw std::runtime_error("Failed to create pipeline state: " + describe(error));
    }

    _pipeline = pso;

    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = YES;
    _depthStencilState = [_device newDepthStencilStateWithDescriptor:depthDesc];
}

void m_pipeline_state::cleanup()
{
    _device = nil;
    _pipeline = nil;
}
