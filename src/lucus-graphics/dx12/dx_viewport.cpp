#include "dx_viewport.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void dx_viewport::init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, window* window)
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

        // m_frame_index = m_swapchain->GetCurrentBackBufferIndex();
	
    mViewport.TopLeftX = 0.0f;
    mViewport.TopLeftY = 0.0f;
    mViewport.Width = (float)width;
    mViewport.Height = (float)height;
    mViewport.MinDepth = 0.0f;
    mViewport.MaxDepth = 1.0f;

    mScissorRect.left = 0;
    mScissorRect.top = 0;
    mScissorRect.right = width;
    mScissorRect.bottom = height;

    // std::printf("Created %zu image views\n", imageViews.size());

    createRTVHeaps();
    createRenderTargets();
}

void dx_viewport::cleanup()
{
    mDXGIFactory.Reset();
    mCommandQueue.Reset();
    swapChain.Reset();
}

void dx_viewport::createRTVHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = g_swapchainImageCount;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&mRTVHeap)), "Failed Create Descriptor Heap");

    mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    std::printf("RTVHeap created successfully\n");
}

void dx_viewport::createRenderTargets()
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