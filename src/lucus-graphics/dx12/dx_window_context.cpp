#include "dx_window_context.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void dx_window_context::init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, window* window)
{
    mDXGIFactory = factory;
    mDevice = device;
    mCommandQueue = commandQueue;

    assert(window);

    HWND hwnd = glfwGetWin32Window(window->getGLFWwindow());
    std::printf("HWND created successfully\n");

    int width = window->framebuffer_width();
    int height = window->framebuffer_height();

    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.BufferCount = g_swapchainImageCount;
    swap_desc.Width = width;
    swap_desc.Height = height;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_desc.SampleDesc.Count = 1;

    Com<IDXGISwapChain1> swapchain1;
    ThrowIfFailed(
        mDXGIFactory->CreateSwapChainForHwnd(
            mCommandQueue.Get(),
            hwnd,
            &swap_desc,
            nullptr,
            nullptr,
            &swapchain1
        ),
        "Failed Create SwapChain for Hwnd"
    );

    ThrowIfFailed(swapchain1.As(&swapChain), "Failed move SwapChain");

    std::printf("IDXGISwapChain1 created successfully (%dx%d)\n", width, height);

    // backBufferIndex = swapChain->GetCurrentBackBufferIndex();

    viewport.viewport.TopLeftX = 0.0f;
    viewport.viewport.TopLeftY = 0.0f;
    viewport.viewport.Width = static_cast<float>(width);
    viewport.viewport.Height = static_cast<float>(height);
    viewport.viewport.MinDepth = 0.0f;
    viewport.viewport.MaxDepth = 1.0f;

    viewport.scissor.left = 0;
    viewport.scissor.top = 0;
    viewport.scissor.right = width;
    viewport.scissor.bottom = height;

    createRTVHeaps();
    createDSVHeap();
    createRenderTargets();
    createDepthStencils();
    createSyncObjects();
}



void dx_window_context::cleanup()
{
    for (int i = 0; i < mDepthStencils.size(); ++i)
    {
        mDepthStencils[i].Reset();
    }

    uniformbuffers.cleanup();

    fence.Reset();
    if (fenceEvent) {
        CloseHandle((HANDLE)fenceEvent);
        fenceEvent = nullptr;
    }

    for (int i = 0; i < mRenderTargets.size(); ++i)
    {
        mRenderTargets[i].Reset();
    }

    mDSVHeap.Reset();
    mRTVHeap.Reset();
    mDXGIFactory.Reset();
    mCommandQueue.Reset();
    swapChain.Reset();
}

void dx_window_context::createRTVHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = g_swapchainImageCount;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&mRTVHeap)), "Failed Create Descriptor Heap");

    mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    std::printf("RTVHeap created successfully\n");
}

void dx_window_context::createDSVHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&mDSVHeap)), "Failed Create Descriptor Heap");

    mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    std::printf("DSVHeap created successfully\n");
}

void dx_window_context::createRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = mRTVHeap->GetCPUDescriptorHandleForHeapStart();

    for (int i = 0; i < mRenderTargets.size(); ++i)
    {
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTargets[i])), "Failed Create Buffer for Render Targets");

        mDevice->CreateRenderTargetView(mRenderTargets[i].Get(), nullptr, handle);
        handle.ptr += mRTVDescriptorSize;
    }

    std::printf("RenderTargets created successfully\n");
}

void dx_window_context::createDepthStencils()
{
    D3D12_RESOURCE_DESC depth_desc{};
    depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_desc.Alignment = 0;
    depth_desc.Width = static_cast<UINT64>(viewport.viewport.Width);
    depth_desc.Height = static_cast<UINT>(viewport.viewport.Height);
    depth_desc.DepthOrArraySize = 1;
    depth_desc.MipLevels = 1;
    depth_desc.Format = mDepthFormat;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_depth_desc{};
    dsv_depth_desc.Format = mDepthFormat;
    dsv_depth_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_depth_desc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE clear_value{};
    clear_value.Format = mDepthFormat;
    clear_value.DepthStencil.Depth = 1.0f;
    clear_value.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    for (int i = 0; i < mDepthStencils.size(); ++i)
    {
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depth_desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value,
            IID_PPV_ARGS(&mDepthStencils[i])
        ), "Failed Create Committed Resource for Depth Stencil");

        mDevice->CreateDepthStencilView(
            mDepthStencils[i].Get(),
            &dsv_depth_desc,
            mDSVHeap->GetCPUDescriptorHandleForHeapStart());
    }

    std::printf("Depth Stencils created successfully\n");
}

void dx_window_context::createSyncObjects()
{
    // Create synchronization objects.
	ThrowIfFailed(mDevice->CreateFence(fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "Failed Create Fence");
	fenceValues[0]++;

	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), " Failed Create Event");
	}

    std::printf("ID3D12Fence created successfully\n");
}