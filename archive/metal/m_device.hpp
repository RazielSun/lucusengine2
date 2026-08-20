#pragma once

#include "m_pch.hpp"

namespace lucus
{
    class m_device
    {
        public:
            m_device();
            ~m_device();

            id<MTLDevice> getDevice() const { return _device; }
            id<MTLCommandQueue> getCommandPool() const { return _commandPool; }

            void createLogicalDevice();

        private:
            id<MTLDevice> _device;
            id<MTLCommandQueue> _commandPool;
    };
}