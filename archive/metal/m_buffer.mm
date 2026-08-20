#include "m_buffer.hpp"

using namespace lucus;

void m_buffer::init(id<MTLDevice> device, size_t size, u32 count, shader_binding_stage in_stage, MTLResourceOptions options)
{
    assert(device);

    stage = in_stage;

    itemSize = size;
    bufferSize = size * count;
    alignedSize = (bufferSize + 255) & ~255;
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
    assert(size + offset <= _buffer.length);

    void* bufferPointer = _buffer.contents;
    if (!bufferPointer)
    {
        throw std::runtime_error("Failed to get buffer contents pointer");
    }

    std::memcpy((u8*)bufferPointer + offset, data, size);
}

void* m_buffer::getMappedData(u32 index) const
{
    void* bufferPointer = _buffer.contents;
    if (!bufferPointer)
    {
        throw std::runtime_error("Failed to get buffer contents pointer");
    }
    const size_t offset = itemSize * index;
    assert(itemSize + offset <= _buffer.length);
    u8* ptr = (u8*)bufferPointer + offset;
    return (void*)ptr;
}

void m_buffer::cleanup()
{
    _buffer = nil;
}