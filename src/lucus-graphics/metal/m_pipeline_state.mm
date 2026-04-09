#include "m_pipeline_state.hpp"

#include "filesystem.hpp"
#include "material.hpp"
#include "render_types.hpp"

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

void m_pipeline_state::init(material* mat, MTLPixelFormat colorFormat, MTLPixelFormat depthFormat)
{
    assert(mat && "Material pointer cannot be null");
    id<MTLLibrary> library = loadLibrary(mat->getShaderName());

    id<MTLFunction> vs = [library newFunctionWithName:@"vsMain"];
    id<MTLFunction> fs = [library newFunctionWithName:@"psMain"];
    if (!vs || !fs) {
        throw std::runtime_error("Failed to load shader functions");
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vs;
    desc.fragmentFunction = fs;
    desc.colorAttachments[0].pixelFormat = colorFormat;
    desc.depthAttachmentPixelFormat = depthFormat;

    if (mat->isUseVertexIndexBuffers())
    {
        NSUInteger bufferIndex = (NSUInteger)shader_binding::vertex;
        MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];

        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
        vertexDescriptor.attributes[0].offset = offsetof(vertex, position);
        vertexDescriptor.attributes[0].bufferIndex = bufferIndex;

        vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        vertexDescriptor.attributes[1].offset = offsetof(vertex, texCoords);
        vertexDescriptor.attributes[1].bufferIndex = bufferIndex;

        vertexDescriptor.attributes[2].format = MTLVertexFormatFloat3;
        vertexDescriptor.attributes[2].offset = offsetof(vertex, color);
        vertexDescriptor.attributes[2].bufferIndex = bufferIndex;

        vertexDescriptor.layouts[bufferIndex].stride = sizeof(vertex);

        desc.vertexDescriptor = vertexDescriptor;
    }
    
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

    _useUniformBuffers = mat->isUseUniformBuffers();

    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = YES;
    _depthStencilState = [_device newDepthStencilStateWithDescriptor:depthDesc];
}

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
