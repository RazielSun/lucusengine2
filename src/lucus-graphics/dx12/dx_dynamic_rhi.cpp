#include "dx_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "material.hpp"

#include "glfw_include.hpp"

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
    _frameUniformBuffer.cleanup();
    for (auto& buffer : _objectUniformBuffers) {
        buffer.cleanup();
    }
    _objectUniformBuffers.clear();

    _fence.Reset();

    _pipelineStates.clear();

    _commandbuffer_pool.cleanup();

    for (auto& viewport : _viewports) {
        viewport.cleanup();
    }
    _viewports.clear();

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
    createSyncObjectsStable();
    createFrameUniformBuffers();
}

viewport_handle dx_dynamic_rhi::createViewport(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to dx_swapchain::init");
	}

    dx_viewport viewport;
    viewport.init(_DXGIFactory, _deviceHandle, _commandQueue, window);

    _viewports.push_back(viewport);
    viewport_handle out_handle(static_cast<uint32_t>(_viewports.size()));

    return out_handle;
}

void dx_dynamic_rhi::beginFrame(const viewport_handle& handle)
{
    if (!handle.is_valid()) {
        return;
    }
    
    _currentViewport = handle;
    // dx_viewport& viewport = _viewports[_currentViewport.as_index()];

    uint32_t bufferIndex = _currentFrame % g_framesInFlight;

    ThrowIfFailed(_commandbuffer_pool.commandAllocators[bufferIndex]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(_commandBuffer->Reset(_commandbuffer_pool.commandAllocators[bufferIndex].Get(), nullptr), "Failed Reset Command Buffer");
}

void dx_dynamic_rhi::endFrame()
{
    if (!_currentViewport.is_valid()) {
        return;
    }

    dx_viewport& viewport = _viewports[_currentViewport.as_index()];

    uint32_t fenceIndex = _currentFrame % g_framesInFlight;
    const uint64_t current_fence = _fenceValues[fenceIndex];

    ThrowIfFailed(_commandQueue->Signal(_fence.Get(), current_fence), "Failed Queue Signal");

    _currentFrame = viewport.swapChain->GetCurrentBackBufferIndex();

    if (_fence->GetCompletedValue() < _fenceValues[fenceIndex])
    {
        ThrowIfFailed(_fence->SetEventOnCompletion(_fenceValues[fenceIndex], (HANDLE)_fenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)_fenceEvent, INFINITE);
    }

    _fenceValues[fenceIndex] = current_fence + 1;
}

void dx_dynamic_rhi::submit(const command_buffer& cmd)
{
    if (!_currentViewport.is_valid()) {
        return;
    }

    dx_viewport& viewport = _viewports[_currentViewport.as_index()];

    if (!viewport.mRenderTargets[_currentFrame])
        throw std::runtime_error("Current render target is null");

    if (!_commandBuffer)
        throw std::runtime_error("Command buffer is null");

    if (!viewport.mRTVHeap)
        throw std::runtime_error("RTV heap is null");

    D3D12_RESOURCE_BARRIER barrier_begin{};
    barrier_begin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_begin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_begin.Transition.pResource = viewport.mRenderTargets[_currentFrame].Get();
    barrier_begin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_begin.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_begin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    _commandBuffer->ResourceBarrier(1, &barrier_begin);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = viewport.mRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(_currentFrame) * viewport.mRTVDescriptorSize;

    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    _commandBuffer->RSSetViewports(1, &viewport.mViewport);
    _commandBuffer->RSSetScissorRects(1, &viewport.mScissorRect);
    _commandBuffer->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    _commandBuffer->ClearRenderTargetView(rtv, clear_color, 0, nullptr);

    uint32_t bufferIndex = _currentFrame % g_framesInFlight;

    _frameUniformBuffer.write(bufferIndex, &cmd.frame_ubo, sizeof(cmd.frame_ubo));

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
                    cmd->SetGraphicsRootConstantBufferView(0, frameCB[currentFrame]->GetGPUVirtualAddress());
                    cmd->SetGraphicsRootConstantBufferView(1, objectCBs[object_id.as_index()][currentFrame]->GetGPUVirtualAddress());
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
    barrier_end.Transition.pResource = viewport.mRenderTargets[_currentFrame].Get();
    barrier_end.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_end.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier_end.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    _commandBuffer->ResourceBarrier(1, &barrier_end);

    ThrowIfFailed(_commandBuffer->Close(), "Failed Close Command Buffer");

    ID3D12CommandList* command_lists[] = { _commandBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, command_lists);

    ThrowIfFailed(viewport.swapChain->Present(1, 0), "Failed SwapChain Preset");
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

    // int renderPassIndex = mat->getRenderPass();
    // if (renderPassIndex < 0 || renderPassIndex >= static_cast<int>(_renderPasses.size())) {
    //     std::cerr << "Invalid render pass index in material: " << renderPassIndex << std::endl;
    //     return material_handle();
    // }
    
    it->second.init(mat, mat->isUseUniformBuffers() ? 2 : 0);

    return material_handle(shaderHash);
}

render_object_handle dx_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    assert(obj != nullptr);

    _objectUniformBuffers.emplace_back();
    dx_buffer& buffer = _objectUniformBuffers.back();
    buffer.init(_deviceHandle, sizeof(uniform_buffer));

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
    _commandbuffer_pool.init(_deviceHandle);

    // Create command list for recording graphics commands
	ThrowIfFailed(_deviceHandle->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, _commandbuffer_pool.commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(_commandBuffer.ReleaseAndGetAddressOf())),
        "Failed Create Command List"
    );
	ThrowIfFailed(_commandBuffer->Close(), "Failed Close Command List");
	_commandBuffer->SetName(L"Command List");

    std::printf("ID3D12GraphicsCommandList created successfully\n");
}

void dx_dynamic_rhi::createSyncObjectsStable()
{
    // Create synchronization objects.
	ThrowIfFailed(_deviceHandle->CreateFence(_fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)), "Failed Create Fence");
	_fenceValues[0]++;

	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), " Failed Create Event");
	}

    std::printf("ID3D12Fence created successfully\n");
}

void dx_dynamic_rhi::createFrameUniformBuffers()
{
    _frameUniformBuffer.init(_deviceHandle, sizeof(frame_uniform_buffer));

    std::printf("Frame uniform buffer created successfully\n");
}

void dx_dynamic_rhi::wait_idle()
{
    ThrowIfFailed(
        _commandQueue->Signal(_fence.Get(), _fenceValues[_currentFrame]),
        "wait_for_gpu signal failed"
    );

    ThrowIfFailed(
        _fence->SetEventOnCompletion(_fenceValues[_currentFrame], (HANDLE)_fenceEvent),
        "wait_for_gpu SetEventOnCompletion failed"
    );

    WaitForSingleObject((HANDLE)_fenceEvent, INFINITE);
    ++_fenceValues[_currentFrame];
}