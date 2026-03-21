#include "dx_device.hpp"

using namespace lucus;

dx_device::dx_device(IDXGIAdapter1* gpuAdapter)
    : mGPU(gpuAdapter)
{
}

dx_device::~dx_device()
{
    mDevice.Reset();
    mGPU.Reset();
}

void dx_device::createLogicalDevice(D3D_FEATURE_LEVEL featureLevel)
{
    ThrowIfFailed(D3D12CreateDevice(mGPU.Get(), featureLevel, IID_PPV_ARGS(&mDevice)), "Failed Create Device");

    std::printf("ID3D12Device created successfully\n");
}