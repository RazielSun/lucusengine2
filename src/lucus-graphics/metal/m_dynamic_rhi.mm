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
    createSyncObjectsStable();
    createFrameUniformBuffers();
}

viewport_handle m_dynamic_rhi::createViewport(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    m_viewport viewport;
    viewport.init(_deviceHandle, window);

    _viewports.push_back(viewport);

    viewport_handle out_handle(static_cast<uint32_t>(_viewports.size()));
    return out_handle;
}

void m_dynamic_rhi::beginFrame(const viewport_handle& handle)
{
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    if (!handle.is_valid()) {
        return;
    }

    _currentViewport = handle;
    m_viewport& viewport = _viewports[_currentViewport.as_index()];

    _currentDrawable = viewport.getNewDrawable();
}

void m_dynamic_rhi::endFrame()
{
    if (!_currentDrawable || !_currentBuffer)
    {
        return;
    }

    [_currentBuffer presentDrawable:_currentDrawable];
    [_currentBuffer commit];

    _currentBufferIndex = (_currentBufferIndex + 1) % g_framesInFlight;
}

void m_dynamic_rhi::submit(const command_buffer& cmd)
{
    if (!_currentDrawable)
    {
        dispatch_semaphore_signal(_frameSemaphore);
        return;
    }

    _currentBuffer = [_device->getCommandPool() commandBuffer];

    dispatch_semaphore_t semaphore = _frameSemaphore;
    [_currentBuffer addCompletedHandler:^(id<MTLCommandBuffer> _) {
        dispatch_semaphore_signal(semaphore);
    }];

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = _currentDrawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    size_t frameUniformOffset = _currentBufferIndex * sizeof(frame_uniform_buffer);
    memcpy((uint8_t*)_frameUniformBuffers.contents + frameUniformOffset, &cmd.frame_ubo, sizeof(cmd.frame_ubo));

    for (const auto& renderInstance : cmd.render_list) {
        id<MTLRenderCommandEncoder> enc = [_currentBuffer renderCommandEncoderWithDescriptor:pass];

        const auto& object_id = renderInstance.object;

        size_t objectSize = sizeof(renderInstance.object_data);
        size_t objectUniformOffset = objectSize * object_id.as_index();
        memcpy((uint8_t*)_objectUniformBuffers[_currentBufferIndex].contents + objectUniformOffset, &renderInstance.object_data, objectSize);

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end())
            {
                [enc setRenderPipelineState:psoIt->second.getPipeline()];
                if (psoIt->second.isUniformBufferUsed())
                {
                    [enc setVertexBuffer:_frameUniformBuffers offset:frameUniformOffset atIndex:0];
                    [enc setVertexBuffer:_objectUniformBuffers[_currentBufferIndex] offset:objectUniformOffset atIndex:1];
                }
                else
                {
                    std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
                }
            }
        }

        const auto& mesh = renderInstance.mesh;
        // TODO: Bind vertex/index buffers and draw
        // TODO: mesh_handle = mesh.getDrawCount() is for test only
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:mesh.get()];
        [enc endEncoding];
    }
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

    // int renderPassIndex = mat->getRenderPass();
    // if (renderPassIndex < 0 || renderPassIndex >= static_cast<int>(_renderPasses.size())) {
    //     std::cerr << "Invalid render pass index in material: " << renderPassIndex << std::endl;
    //     return material_handle();
    // }

    // TODO: !    
    it->second.init(mat, _viewports[0].getPixelFormat());

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

void m_dynamic_rhi::createSyncObjectsStable()
{
    _frameSemaphore = dispatch_semaphore_create(g_framesInFlight);
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