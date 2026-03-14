#include "dx_swapchain.hpp"

#include "window.hpp"

#include "render_config.hpp"
#include "glfw_include.hpp"

using namespace lucus;

dx_swapchain::~dx_swapchain()
{
    mDXGIFactory.Reset();
    mCommandQueue.Reset();
    mSwapChain.Reset();
}

void dx_swapchain::setContext(Com<IDXGIFactory4> factory, Com<ID3D12CommandQueue> commandQueue)
{
    mDXGIFactory = factory;
    mCommandQueue = commandQueue;
}

void dx_swapchain::createSurface(std::shared_ptr<window> window)
{
    _hwnd = glfwGetWin32Window(window->getGLFWwindow());

    std::printf("HWND created successfully\n");
}

void dx_swapchain::create(std::shared_ptr<window> window)
{
    // assert(_gpu);
	// assert(_device);
	// assert(_instance);

    int width = window->framebuffer_width();
    int height = window->framebuffer_height();

    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.BufferCount = g_maxConcurrentFrames;
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
            _hwnd,
            &swap_desc,
            nullptr,
            nullptr,
            &swapchain1
        ),
        "Failed Create SwapChain for Hwnd"
    );

    ThrowIfFailed(swapchain1.As(&mSwapChain), "Failed move SwapChain");

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
}