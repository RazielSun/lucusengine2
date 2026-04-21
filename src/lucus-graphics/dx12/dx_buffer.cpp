#include "dx_buffer.hpp"

using namespace lucus;

void dx_buffer::init(Com<ID3D12Device> device, size_t bufferSize)
{
    _device = device;

    size_t alignedSize = (bufferSize + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

    ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_buffer)), "Failed to create buffer");

    // Map the buffers.
    ThrowIfFailed(_buffer->Map(0, nullptr, reinterpret_cast<void**>(&_mapped)), "Failed to map buffer");
}

void dx_buffer::cleanup()
{
    if (_buffer) {
        _buffer->Unmap(0, nullptr);
        _buffer.Reset();
    }
    _mapped = nullptr;
}

void dx_buffer::write(const void* data, size_t size, size_t offset)
{
    if (!_mapped) {
        throw std::runtime_error("Buffer is not mapped");
    }
    memcpy(static_cast<uint8_t*>(_mapped) + offset, data, size);
}

void dx_uniform_buffer::init(Com<ID3D12Device> device, size_t bufferSize)
{
    for (size_t i = 0; i < g_framesInFlight; ++i)
    {
        _buffers[i].init(device, bufferSize);
    }
}

void dx_uniform_buffer::cleanup()
{
    for (size_t i = 0; i < g_framesInFlight; ++i)
    {
        _buffers[i].cleanup();
    }
}

void dx_uniform_buffer::write(u32 index, const void* data, size_t size, size_t offset)
{
    _buffers[index].write(data, size, offset);
}