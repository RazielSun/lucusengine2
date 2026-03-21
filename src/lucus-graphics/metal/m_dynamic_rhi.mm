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
    _pipeline = nil;
    _metalLayer = nil;
    _deviceHandle = nil;
    _device.reset();
}

void m_dynamic_rhi::init()
{
    createDevice();
    createSyncObjectsStable();
}

viewport_handle m_dynamic_rhi::createViewport(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    m_viewport viewport;
    viewport.init(_deviceHandle, window);

    int viewport_id = static_cast<int>(_viewports.size());
    _viewports.push_back(viewport);

    viewport_handle out_handle(viewport_id);
    return out_handle;
}

void m_dynamic_rhi::beginFrame(const viewport_handle& viewport)
{
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    if (!handle.is_valid()) {
        return;
    }

    _currentViewport = handle;
    m_viewport& viewport = _viewports[_currentViewport.get()];

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

    for (const auto& renderInstance : cmd.render_list) {
        id<MTLRenderCommandEncoder> enc = [_currentBuffer renderCommandEncoderWithDescriptor:pass];

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end()) {
                [enc setRenderPipelineState:psoIt->second.getPipeline()];
            } else {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }

        auto mesh = renderInstance.mesh;
        // TODO: Bind vertex/index buffers and draw
        // vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        [enc drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:0
            vertexCount:3];

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
    it->second.init(shaderName, _viewports[0].GetPixelFormat());

    return material_handle(shaderHash);
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