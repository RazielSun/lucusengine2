#include "dx_device.hpp"

using namespace lucus;

dx_device::dx_device(IDXGIAdapter1* gpuAdapter)
    : mGPU(gpuAdapter)
{
}

dx_device::~dx_device()
{
    // for (auto& commandAllocator : mCommandAllocators)
    // {
    //     commandAllocator.Reset();
    // }
    mDevice.Reset();
    mGPU.Reset();
}

void dx_device::createLogicalDevice(D3D_FEATURE_LEVEL featureLevel)
{
    ThrowIfFailed(D3D12CreateDevice(mGPU.Get(), featureLevel, IID_PPV_ARGS(&mDevice)), "Failed Create Device");

    std::printf("ID3D12Device created successfully\n");

    // // [Command Pool] Create command allocators for each back buffer
	// for (auto& commandAllocator : mCommandAllocators)
	// {
	// 	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf())), "Failed Create Command Allocator");
	// }

    // std::printf("ID3D12CommandAllocators created successfully\n");
}