#include "m_device.hpp"

using namespace lucus;

m_device::m_device() : _device(nil), _commandPool(nil)
{
}

m_device::~m_device()
{
    _device = nil;
    _commandPool = nil;
}

void m_device::createLogicalDevice()
{
    _device = MTLCreateSystemDefaultDevice();
    if (!_device) {
        throw std::runtime_error("failed to create Metal device!");
    }

    std::printf("MTLDevice created successfully\n");
    
    _commandPool = [_device newCommandQueue];
    if (!_commandPool) {
        throw std::runtime_error("failed to create Metal command queue!");
    }

    std::printf("MTLCommandQueue created successfully\n");
}
