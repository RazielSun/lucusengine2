#include "dx_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "material.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<dx_dynamic_rhi>();
    }
}

using namespace lucus;

dx_dynamic_rhi::dx_dynamic_rhi()
{
}

dx_dynamic_rhi::~dx_dynamic_rhi()
{
    for (auto& buffer : _objectUniformBuffers) {
        buffer.cleanup();
    }
    _objectUniformBuffers.clear();

    _pipelineStates.clear();

    _commandBuffer.Reset();
    _commandPool.cleanup();

    for (auto& context : _contexts) {
        context.cleanup();
    }
    _contexts.clear();

    _commandQueue.Reset();
    _deviceHandle.Reset();
    _device.reset();
    _DXGIFactory.Reset();
}

void dx_dynamic_rhi::init()
{
    createInstance();
    createDevice();
    createCommandBufferPool();
}

window_context_handle dx_dynamic_rhi::createWindowContext(const window_handle& handle) 
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to dx_swapchain::init");
	}

    dx_window_context context;
    context.init(_DXGIFactory, _deviceHandle, _commandQueue, window);
    context.uniformbuffers.init(_deviceHandle, sizeof(frame_uniform_buffer));

    _contexts.push_back(context);
    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

const std::vector<window_context_handle>& dx_dynamic_rhi::getWindowContexts() const
{
    return _contextHandles;
}

float dx_dynamic_rhi::getWindowContextAspectRatio(const window_context_handle& ctx_handle) const
{
    if (!ctx_handle.is_valid()) {
        return 1.0f;
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    return static_cast<float>(ctx.viewport.viewport.Width) / static_cast<float>(ctx.viewport.viewport.Height);
}

void dx_dynamic_rhi::beginFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    // Nothing?
}

void dx_dynamic_rhi::submit(const window_context_handle& ctx_handle, const command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    uint32_t bufferIndex = ctx.backBufferIndex % g_framesInFlight;

    // Here or in beginFrame?
    ThrowIfFailed(_commandPool.commandAllocators[bufferIndex]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(_commandBuffer->Reset(_commandPool.commandAllocators[bufferIndex].Get(), nullptr), "Failed Reset Command Buffer");
    
    if (!_commandBuffer)
        throw std::runtime_error("Command buffer is null");

    if (!ctx.mRTVHeap)
        throw std::runtime_error("RTV heap is null");
    if (!ctx.mRenderTargets[ctx.backBufferIndex])
        throw std::runtime_error("Current render target is null");

    if (!ctx.mDSVHeap)
        throw std::runtime_error("DSV heap is null");
    if (!ctx.mDepthStencils[bufferIndex])
        throw std::runtime_error("Depth stencil is null");

    D3D12_RESOURCE_BARRIER barrier_begin{};
    barrier_begin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_begin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_begin.Transition.pResource = ctx.mRenderTargets[ctx.backBufferIndex].Get();
    barrier_begin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_begin.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_begin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    _commandBuffer->ResourceBarrier(1, &barrier_begin);

    _commandBuffer->RSSetViewports(1, &ctx.viewport.viewport);
    _commandBuffer->RSSetScissorRects(1, &ctx.viewport.scissor);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = ctx.mRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(ctx.backBufferIndex) * ctx.mRTVDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = ctx.mDSVHeap->GetCPUDescriptorHandleForHeapStart();
    dsv.ptr += static_cast<SIZE_T>(bufferIndex) * ctx.mDSVDescriptorSize;

    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    _commandBuffer->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    _commandBuffer->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
    _commandBuffer->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ctx.uniformbuffers.write(bufferIndex, &cmd.frame_ubo, sizeof(cmd.frame_ubo));

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;

        dx_buffer& buffer = _objectUniformBuffers[object_id.as_index()];
        buffer.write(bufferIndex, &renderInstance.object_data, sizeof(renderInstance.object_data));

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end())
            {
                _commandBuffer->SetGraphicsRootSignature(psoIt->second.getRootSignature().Get());
                _commandBuffer->SetPipelineState(psoIt->second.getPipeline().Get());
                if (psoIt->second.isUniformBufferUsed())
                {
                    _commandBuffer->SetGraphicsRootConstantBufferView(0, ctx.uniformbuffers.get(bufferIndex)->GetGPUVirtualAddress());
                    _commandBuffer->SetGraphicsRootConstantBufferView(1, _objectUniformBuffers[object_id.as_index()].get(bufferIndex)->GetGPUVirtualAddress());
                }
            }
            else
            {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }
        
        const auto& mesh = renderInstance.mesh;
        // TODO: Bind vertex/index buffers and draw
        _commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // TODO: mesh_handle = mesh.getDrawCount() is for test only
        _commandBuffer->DrawInstanced(mesh.get(), 1, 0, 0);
    }

    D3D12_RESOURCE_BARRIER barrier_end{};
    barrier_end.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_end.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_end.Transition.pResource = ctx.mRenderTargets[ctx.backBufferIndex].Get();
    barrier_end.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_end.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier_end.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    _commandBuffer->ResourceBarrier(1, &barrier_end);

    ThrowIfFailed(_commandBuffer->Close(), "Failed Close Command Buffer");

    ID3D12CommandList* command_lists[] = { _commandBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, command_lists);

    ThrowIfFailed(ctx.swapChain->Present(1, 0), "Failed SwapChain Preset");
}

void dx_dynamic_rhi::endFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    uint32_t fenceIndex = ctx.backBufferIndex % g_framesInFlight;
    const uint64_t current_fence = ctx.fenceValues[fenceIndex];

    ThrowIfFailed(_commandQueue->Signal(ctx.fence.Get(), current_fence), "Failed Queue Signal");

    ctx.backBufferIndex = ctx.swapChain->GetCurrentBackBufferIndex();

    if (ctx.fence->GetCompletedValue() < ctx.fenceValues[fenceIndex])
    {
        ThrowIfFailed(ctx.fence->SetEventOnCompletion(ctx.fenceValues[fenceIndex], (HANDLE)ctx.fenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)ctx.fenceEvent, INFINITE);
    }

    ctx.fenceValues[fenceIndex] = current_fence + 1;
}

material_handle dx_dynamic_rhi::createMaterial(material* mat)
{
    assert(mat != nullptr);

    const std::string& shaderName = mat->getShaderName();
    uint32_t shaderHash = std::hash<std::string>{}(shaderName);

    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        return material_handle(it->first);
    }

    dx_pipeline_state pipeline_state(_deviceHandle);
    _pipelineStates.emplace(shaderHash, pipeline_state);

    it = _pipelineStates.find(shaderHash);
    
    // TODO: depth format should be determined by window context or material properties, not hardcoded
    it->second.init(mat, _contexts[0].mDepthFormat, mat->isUseUniformBuffers() ? 2 : 0);

    return material_handle(shaderHash);
}

render_object_handle dx_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    assert(obj != nullptr);

    _objectUniformBuffers.emplace_back();
    dx_buffer& buffer = _objectUniformBuffers.back();
    buffer.init(_deviceHandle, sizeof(frame_uniform_buffer));

    std::printf("Uniform buffer for object %p created successfully\n", obj);

    return render_object_handle(static_cast<uint32_t>(_objectUniformBuffers.size()));
}

void dx_dynamic_rhi::createInstance()
{
    UINT createFactoryFlags = 0;
#ifndef NDEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&_DXGIFactory)), "Failed Create Factory");

    std::printf("IDXGIFactory4 created successfully\n");
}

void dx_dynamic_rhi::createDevice()
{
    #ifndef NDEBUG
	{
		// Enable debug layer interface before create dx device
		Com<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)), "Failed Get Debug Interface");
		debugController->EnableDebugLayer();

        Com<ID3D12Debug1> debugController1;
        ThrowIfFailed(debugController.As(&debugController1), "Failed As Debug Interface");
        debugController1->SetEnableGPUBasedValidation(TRUE);
	}
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    Com<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != _DXGIFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), featureLevel, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

    _device = std::make_unique<dx_device>(adapter.Get());
    _device->createLogicalDevice(featureLevel);

    _deviceHandle = _device->getDevice();

    // Here was an allocator of cmd pool

    // Enable debug messages in debug mode.
#ifndef NDEBUG
    Com<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(_deviceHandle.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};
 
        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };
 
        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };
 
        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;
 
        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter), "Failed Push Debug Info");
    }
#endif

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(_deviceHandle->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.ReleaseAndGetAddressOf())), "Failed Create Command Queue");
	_commandQueue->SetName(L"Command Queue");

    // _swapchain->setContext(mDXGIFactory, _commandQueue);

    std::printf("ID3D12CommandQueue created successfully\n");
}

void dx_dynamic_rhi::createCommandBufferPool()
{
    _commandPool.init(_deviceHandle);

    // Create command list for recording graphics commands
	ThrowIfFailed(_deviceHandle->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, _commandPool.commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(_commandBuffer.ReleaseAndGetAddressOf())),
        "Failed Create Command List"
    );
	ThrowIfFailed(_commandBuffer->Close(), "Failed Close Command List");
	_commandBuffer->SetName(L"Command List");

    std::printf("ID3D12GraphicsCommandList created successfully\n");
}

// void dx_dynamic_rhi::wait_idle()
// {
//     ThrowIfFailed(
//         _commandQueue->Signal(_fence.Get(), _fenceValues[_currentFrame]),
//         "wait_for_gpu signal failed"
//     );

//     ThrowIfFailed(
//         _fence->SetEventOnCompletion(_fenceValues[_currentFrame], (HANDLE)_fenceEvent),
//         "wait_for_gpu SetEventOnCompletion failed"
//     );

//     WaitForSingleObject((HANDLE)_fenceEvent, INFINITE);
//     ++_fenceValues[_currentFrame];
// }