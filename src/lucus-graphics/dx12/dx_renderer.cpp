#include "dx_renderer.hpp"

#include "filesystem.hpp"

std::shared_ptr<lucus::renderer> lucus::create_renderer()
{
    auto renderer = std::make_shared<lucus::dx_renderer>();
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
}

using namespace lucus;

dx_renderer::dx_renderer()
{
    _swapchain = std::make_unique<dx_swapchain>();
}

bool dx_renderer::init()
{
    createInstance();
    initDevices();
    return true;
}

bool dx_renderer::prepare(std::shared_ptr<window> window)
{
    _swapchain->createSurface(window);
    _swapchain->create(window);
    createCommandBuffers();
    createSyncObjects();
    createRTVHeaps();
    createRenderTargets();
    createRootSignature();
    createGraphicsPipeline();
    return true;
}

void dx_renderer::tick()
{
    prepareFrame();
    buildCommandBuffer();
    submitFrame();
}

void dx_renderer::cleanup()
{
    // TODO: destroy other
    destroySyncObjects();
    destroyCommandBuffers();
    _swapchain.reset();
    mCommandQueue.Reset();
    deviceDX.Reset();
    _device.reset();
    mDXGIFactory.Reset();
}

void dx_renderer::createInstance()
{
    UINT createFactoryFlags = 0;
#ifndef NDEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&mDXGIFactory)), "Failed Create Factory");

    std::printf("IDXGIFactory4 created successfully\n");
}

void dx_renderer::initDevices()
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
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != mDXGIFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
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

    deviceDX = _device->getDevice();

    // [Command Pool] Create command allocators for each back buffer
	for (auto& commandAllocator : mCommandAllocators)
	{
		ThrowIfFailed(deviceDX->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf())), "Failed Create Command Allocator");
	}

    // Enable debug messages in debug mode.
#ifndef NDEBUG
    Com<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(deviceDX.As(&pInfoQueue)))
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

    ThrowIfFailed(deviceDX->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueue.ReleaseAndGetAddressOf())), "Failed Create Command Queue");
	mCommandQueue->SetName(L"Command Queue");

    _swapchain->setContext(mDXGIFactory, mCommandQueue);

    std::printf("ID3D12CommandQueue created successfully\n");
}

void dx_renderer::createCommandBuffers()
{
    // Create command list for recording graphics commands
	ThrowIfFailed(deviceDX->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(mCommandBuffer.ReleaseAndGetAddressOf())),
        "Failed Create Command List"
    );
	ThrowIfFailed(mCommandBuffer->Close(), "Failed Close Command List");
	mCommandBuffer->SetName(L"Command List");

    std::printf("ID3D12GraphicsCommandList created successfully\n");
}

void dx_renderer::destroyCommandBuffers()
{
    // TODO
}

void dx_renderer::createSyncObjects()
{
    // Create synchronization objects.
	ThrowIfFailed(deviceDX->CreateFence(mFenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)), "Failed Create Fence");
	mFenceValues[0]++;

	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (mFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), " Failed Create Event");
	}

    std::printf("ID3D12Fence created successfully\n");
}

void dx_renderer::destroySyncObjects()
{
    // TODO
}

void dx_renderer::createRTVHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = g_maxConcurrentFrames;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(deviceDX->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&mRTVHeap)), "Failed Create Descriptor Heap");

    mRTVDescriptorSize = deviceDX->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    std::printf("RTVHeap created successfully\n");
}

void dx_renderer::createRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = mRTVHeap->GetCPUDescriptorHandleForHeapStart();

    //int i = 0;
    //for (auto& renderTarget : mRenderTargets)
    for (int i = 0; i < mRenderTargets.size(); ++i)
    {
        ThrowIfFailed(_swapchain->getSwapChain()->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])), "Failed Create Buffer for Render Targets");

        deviceDX->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, handle);
        handle.ptr += mRTVDescriptorSize;
    }

    std::printf("RenderTargets created successfully\n");
}

void dx_renderer::createRootSignature()
{
    // Create an empty root signature.
	//with 1 parameter for CB
	// {
	// 	CD3DX12_DESCRIPTOR_RANGE cbRange;
	// 	cbRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	// 	CD3DX12_DESCRIPTOR_RANGE srRange;
	// 	srRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// 	CD3DX12_ROOT_PARAMETER rootParameters[2];
	// 	rootParameters[0].InitAsConstantBufferView(0);
	// 	//rootParameters[0].InitAsDescriptorTable(1, &cbRange, D3D12_SHADER_VISIBILITY_VERTEX);
	// 	rootParameters[1].InitAsDescriptorTable(1, &srRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// 	// Use a static sampler that matches the defaults
	// 	// https://msdn.microsoft.com/en-us/library/windows/desktop/dn913202(v=vs.85).aspx#static_sampler
	// 	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	// 	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	// 	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// 	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// 	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// 	samplerDesc.MaxAnisotropy = 16;
	// 	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// 	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	// 	samplerDesc.MinLOD = 0;
	// 	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	// 	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// 	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	// 	//rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	// 	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 	Com<ID3DBlob> signature;
	// 	Com<ID3DBlob> error;
	// 	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	// 	ThrowIfFailed(deviceDX->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

	// 	mRootSignature->SetName(L"Root Signature");
	// }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Com<ID3DBlob> signature;
    Com<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error
        ),
        "Failed Serialize Root Signature"
    );

    ThrowIfFailed(deviceDX->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignature)
        ),
        "Failed Create Root Signature"
    );

    std::printf("ID3D12RootSignature created successfully\n");
}

void dx_renderer::createGraphicsPipeline()
{
    UINT compile_flags = 0;
#ifndef NDEBUG
    compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Com<ID3DBlob> vs;
    Com<ID3DBlob> ps;
    Com<ID3DBlob> error;

    std::string str_shader_path = filesystem::instance().get_path("shaders/triangle.hlsl");
    std::wstring shader_path = utf8_to_wstring(str_shader_path);

    ThrowIfFailed(D3DCompileFromFile(
            shader_path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain",
            "vs_5_0",
            compile_flags,
            0,
            &vs,
            &error
        ),
        "Failed Compile VS Shader"
    );

    ThrowIfFailed(D3DCompileFromFile(
            shader_path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain",
            "ps_5_0",
            compile_flags,
            0,
            &ps,
            &error
        ),
        "Failed Compile PS Shader"
    );

    D3D12_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC default_rt_blend =
    {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    for (int i = 0; i < 8; ++i)
        blend.RenderTarget[i] = default_rt_blend;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = mRootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.BlendState = blend;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rasterizer;

    D3D12_DEPTH_STENCIL_DESC depth_stencil{};
    depth_stencil.DepthEnable = FALSE;
    depth_stencil.StencilEnable = FALSE;
    pso.DepthStencilState = depth_stencil;

    pso.InputLayout = { nullptr, 0 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(deviceDX->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mPipelineState)), "Failed Create Graphics Pipeline State");

    std::printf("ID3D12PipelineState created successfully\n");
}

void dx_renderer::prepareFrame()
{
    //
}

void dx_renderer::buildCommandBuffer()
{
    // ThrowIfFailed(
    //     mCommandQueue->Signal(mFence.Get(), mFenceValues[_currentFrame]),
    //     "wait_for_gpu signal failed"
    // );

    // ThrowIfFailed(
    //     mFence->SetEventOnCompletion(mFenceValues[_currentFrame], (HANDLE)mFenceEvent),
    //     "wait_for_gpu SetEventOnCompletion failed"
    // );

    // WaitForSingleObject((HANDLE)mFenceEvent, INFINITE);
    // ++mFenceValues[_currentFrame];

    ThrowIfFailed(mCommandAllocators[_currentFrame]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(mCommandBuffer->Reset(mCommandAllocators[_currentFrame].Get(), mPipelineState.Get()), "Failed Reset Command Buffer");

    if (!mRenderTargets[_currentFrame])
        throw std::runtime_error("Current render target is null");

    if (!mCommandBuffer)
        throw std::runtime_error("Command buffer is null");

    if (!mRTVHeap)
        throw std::runtime_error("RTV heap is null");

    D3D12_RESOURCE_BARRIER barrier_begin{};
    barrier_begin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_begin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_begin.Transition.pResource = mRenderTargets[_currentFrame].Get();
    barrier_begin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_begin.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_begin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    mCommandBuffer->ResourceBarrier(1, &barrier_begin);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(_currentFrame) * mRTVDescriptorSize;

    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    mCommandBuffer->RSSetViewports(1, &_swapchain->mViewport);
    mCommandBuffer->RSSetScissorRects(1, &_swapchain->mScissorRect);
    mCommandBuffer->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    mCommandBuffer->ClearRenderTargetView(rtv, clear_color, 0, nullptr);

    mCommandBuffer->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandBuffer->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER barrier_end{};
    barrier_end.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_end.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_end.Transition.pResource = mRenderTargets[_currentFrame].Get();
    barrier_end.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_end.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier_end.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    mCommandBuffer->ResourceBarrier(1, &barrier_end);

    ThrowIfFailed(mCommandBuffer->Close(), "Failed Close Command Buffer");
}

void dx_renderer::submitFrame()
{
    ID3D12CommandList* command_lists[] = { mCommandBuffer.Get() };
    mCommandQueue->ExecuteCommandLists(1, command_lists);

    ThrowIfFailed(_swapchain->getSwapChain()->Present(1, 0), "Failed SwapChain Preset");

    const uint64_t current_fence = mFenceValues[_currentFrame];

    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), current_fence), "Failed Queue Signal");

    _currentFrame = _swapchain->getSwapChain()->GetCurrentBackBufferIndex();

    if (mFence->GetCompletedValue() < mFenceValues[_currentFrame])
    {
        ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[_currentFrame], (HANDLE)mFenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)mFenceEvent, INFINITE);
    }

    mFenceValues[_currentFrame] = current_fence + 1;
}

// void dx12_renderer::wait_for_gpu()
// {
//     throw_if_failed(
//         m_command_queue->Signal(m_fence.Get(), m_fence_values[m_frame_index]),
//         "wait_for_gpu signal failed"
//     );

//     throw_if_failed(
//         m_fence->SetEventOnCompletion(m_fence_values[m_frame_index], (HANDLE)m_fence_event),
//         "wait_for_gpu SetEventOnCompletion failed"
//     );

//     WaitForSingleObject((HANDLE)m_fence_event, INFINITE);
//     ++m_fence_values[m_frame_index];
// }

// void dx12_renderer::wait_idle()
// {
//     wait_for_gpu();
// }