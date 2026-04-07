#include "m_buffer.hpp"

using namespace lucus;

void m_buffer::init(id<MTLDevice> device, size_t size, MTLResourceOptions options)
{
    assert(device);

    size_t alignedSize = (size + 255) & ~255;
    _buffer = [device newBufferWithLength:alignedSize options:options];

    if (!_buffer)
    {
        throw std::runtime_error("Failed to create Metal buffer");
    }
}

void m_buffer::write(const void* data, size_t size, size_t offset)
{
    assert(_buffer);
    assert(data);
    assert(size <= _buffer.length);

    void* bufferPointer = _buffer.contents;
    if (!bufferPointer)
    {
        throw std::runtime_error("Failed to get buffer contents pointer");
    }

    std::memcpy((uint8_t*)bufferPointer + offset, data, size);
}

void m_buffer::cleanup()
{
    _buffer = nil;
}