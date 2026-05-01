#include "dx_window_context.hpp"

#include "window.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void dx_window_context::init(Com<IDXGIFactory4> factory, Com<ID3D12Device> device, Com<ID3D12CommandQueue> commandQueue, DXGI_FORMAT format, window* window)
{
    mDXGIFactory = factory;
    mDevice = device;
    mCommandQueue = commandQueue;

    assert(window);

    HWND hwnd = glfwGetWin32Window(window->getGLFWwindow());
    std::printf("HWND created successfully\n");

    width = window->framebuffer_width();
    height = window->framebuffer_height();

    imageCount = g_swapchainImageCount;

    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.BufferCount = imageCount;
    swap_desc.Width = width;
    swap_desc.Height = height;
    swap_desc.Format = format;
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

    std::printf("IDXGISwapChain1 created successfully (%ux%u)\n", width, height);

    backBufferIndex = swapChain->GetCurrentBackBufferIndex();
}

void lucus::dx_window_context::init_images(dx_render_target &rt)
{
    rt.color.images.resize(imageCount);
    rt.color.states.assign(imageCount, D3D12_RESOURCE_STATE_PRESENT);
    for (u32 i = 0; i < imageCount; ++i)
    {
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&rt.color.images[i])), "Failed Get Buffer for Render Targets");
    }

    rt.color.descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rt.color.initialState = D3D12_RESOURCE_STATE_PRESENT;
    rt.color.bPreinitialized = true;

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = imageCount;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rt.color.heap)), "Failed Create RTV Heap for SwapChain");

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(rt.color.heap->GetCPUDescriptorHandleForHeapStart());
    for (u32 i = 0; i < imageCount; ++i)
    {
        mDevice->CreateRenderTargetView(rt.color.images[i].Get(), nullptr, handle);
        handle.ptr += rt.color.descriptorSize;
    }

    std::printf("SwapChain RTV images initialized (%u)\n", imageCount);
}

void dx_window_context::cleanup()
{
    mDevice.Reset();
    mDXGIFactory.Reset();
    mCommandQueue.Reset();
    swapChain.Reset();
}
