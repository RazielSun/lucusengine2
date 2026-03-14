#pragma once

#include "dx_pch.hpp"

#include "device.hpp"
#include "render_config.hpp"

namespace lucus
{
    class dx_device : public idevice
    {
        public:
            dx_device(IDXGIAdapter1* gpuAdapter);
            virtual ~dx_device() override;

            Com<ID3D12Device> getDevice() { return mDevice; }

            void createLogicalDevice(D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0);

            // std::array<Com<ID3D12CommandAllocator>, g_maxConcurrentFrames>& getCommandAllocators() { return mCommandAllocators; }

        private:
            Com<ID3D12Device> mDevice;
            Com<IDXGIAdapter1> mGPU;

            // std::array<Com<ID3D12CommandAllocator>, g_maxConcurrentFrames> mCommandAllocators{};
    };
}