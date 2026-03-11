#pragma once

#include "m_pch.hpp"

#include "device.hpp"

namespace lucus
{
    class m_device : public idevice
    {
        public:
            m_device();
            virtual ~m_device() override;

            id<MTLDevice> getDevice() const { return _device; }
            id<MTLCommandQueue> getCommandPool() const { return _commandPool; }

            void createLogicalDevice();


        private:
            id<MTLDevice> _device;
            id<MTLCommandQueue> _commandPool;
    };
}