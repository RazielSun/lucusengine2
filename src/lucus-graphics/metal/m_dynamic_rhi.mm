#include "m_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "material.hpp"

#include "m_viewport.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<m_dynamic_rhi>();
    }
}

using namespace lucus;

m_dynamic_rhi::m_dynamic_rhi()
{
}

m_dynamic_rhi::~m_dynamic_rhi()
{
    _deviceHandle = nil;
    _device.reset();
}

void m_dynamic_rhi::init()
{
    createDevice();
    createFrameUniformBuffers();
}

window_context_handle m_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    m_window_context context;
    context.init(_deviceHandle, window);

    _contexts.push_back(context);

    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

const std::vector<window_context_handle>& m_dynamic_rhi::getWindowContexts() const
{
    return _contextHandles;
}

float m_dynamic_rhi::getWindowContextAspectRatio(const window_context_handle& ctx_handle) const
{
    if (!ctx_handle.is_valid()) {
        return 1.0f; // Default aspect ratio for invalid handle
    }

    const m_window_context& context = _contexts[ctx_handle.as_index()];
    CGSize drawableSize = context.getDrawableSize();
    return drawableSize.width / drawableSize.height;
}

void m_dynamic_rhi::beginFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    ctx.wait_frame();
}

void m_dynamic_rhi::submit(const window_context_handle& ctx_handle, const command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    if (!ctx.currentDrawable)
    {
        dispatch_semaphore_signal(ctx.frameSemaphore);
        return;
    }

    id<MTLCommandBuffer> currentBuffer = [_device->getCommandPool() commandBuffer];

    dispatch_semaphore_t semaphore = ctx.frameSemaphore;
    [currentBuffer addCompletedHandler:^(id<MTLCommandBuffer> _) {
        dispatch_semaphore_signal(semaphore);
    }];

    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    descriptor.colorAttachments[0].texture = ctx.currentDrawable.texture;
    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    descriptor.depthAttachment.texture = ctx.depthTextures[ctx.currentBufferIndex];
    descriptor.depthAttachment.loadAction = MTLLoadActionClear;
    descriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    descriptor.depthAttachment.clearDepth = 1.0;
    // descriptor.stencilAttachment.texture = ctx.depthTexture;
    // descriptor.stencilAttachment.storeAction = MTLStoreActionStore;
    // descriptor.stencilAttachment.loadAction = MTLLoadActionClear;
    // descriptor.stencilAttachment.clearStencil = 0;

    id<MTLRenderCommandEncoder> pass = [currentBuffer renderCommandEncoderWithDescriptor:descriptor];

    size_t frameUniformOffset = ctx.currentBufferIndex * sizeof(frame_uniform_buffer);
    memcpy((uint8_t*)_frameUniformBuffers.contents + frameUniformOffset, &cmd.frame_ubo, sizeof(cmd.frame_ubo));

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;

        size_t objectSize = sizeof(renderInstance.object_data);
        size_t objectUniformOffset = objectSize * object_id.as_index();
        memcpy((uint8_t*)_objectUniformBuffers[ctx.currentBufferIndex].contents + objectUniformOffset, &renderInstance.object_data, objectSize);

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end())
            {
                [pass setRenderPipelineState:psoIt->second.getPipeline()];
                [pass setDepthStencilState:psoIt->second.getDepthStencilState()];
                if (psoIt->second.isUniformBufferUsed())
                {
                    [pass setVertexBuffer:_frameUniformBuffers offset:frameUniformOffset atIndex:0];
                    [pass setVertexBuffer:_objectUniformBuffers[ctx.currentBufferIndex] offset:objectUniformOffset atIndex:1];
                }
            }
            else
            {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }

        const auto& mesh = renderInstance.mesh;
        // TODO: Bind vertex/index buffers and draw
        // TODO: mesh_handle = mesh.getDrawCount() is for test only
        [pass drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:mesh.get()];
    }

    [pass endEncoding];

    [currentBuffer presentDrawable:ctx.currentDrawable];
    [currentBuffer commit];
}

void m_dynamic_rhi::endFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];
    
    ctx.currentBufferIndex = (ctx.currentBufferIndex + 1) % g_framesInFlight;
}

material_handle m_dynamic_rhi::createMaterial(material* mat)
{
    const std::string& shaderName = mat->getShaderName();
    uint32_t shaderHash = std::hash<std::string>{}(shaderName);

    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        return material_handle(it->first);
    }

    _pipelineStates.emplace(shaderHash, _deviceHandle);

    it = _pipelineStates.find(shaderHash);

    // TODO: !    
    it->second.init(mat, _contexts[0].getPixelFormat(), _contexts[0].getDepthPixelFormat());

    return material_handle(shaderHash);
}

render_object_handle m_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    static uint32_t nextObjectId = 0;
    uint32_t objectId = ++nextObjectId;
    if (objectId >= g_maxObjectBufferCount) {
        throw std::runtime_error("Exceeded maximum object buffer count");
    }
    return render_object_handle(objectId);
}

void m_dynamic_rhi::createDevice()
{
    // engine_info& engine_info = engine_info::instance();

    _device = std::make_unique<m_device>();
    _device->createLogicalDevice();

    _deviceHandle = _device->getDevice();
}

void m_dynamic_rhi::createFrameUniformBuffers()
{
    size_t frameAlignedSize = (sizeof(frame_uniform_buffer) * g_framesInFlight + 255) & ~255;
    _frameUniformBuffers = [_deviceHandle newBufferWithLength:frameAlignedSize options:MTLResourceStorageModeShared];

    size_t objectAlignedSize = (sizeof(object_uniform_buffer) * g_maxObjectBufferCount + 255) & ~255;
    for (int i = 0; i < g_framesInFlight; ++i)
    {
        _objectUniformBuffers[i] = [_deviceHandle newBufferWithLength:objectAlignedSize options:MTLResourceStorageModeShared];
    }

    std::printf("Frame & Object uniform buffers created successfully\n");
}