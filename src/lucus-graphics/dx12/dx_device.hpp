#pragma once

#include "dx_pch.hpp"

namespace lucus
{
    class dx_device
    {
        public:
            dx_device(IDXGIAdapter1* gpuAdapter);
            virtual ~dx_device();

            Com<ID3D12Device> getDevice() { return mDevice; }

            void createLogicalDevice(D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0);

        private:
            Com<ID3D12Device> mDevice;
            Com<IDXGIAdapter1> mGPU;
    };
}