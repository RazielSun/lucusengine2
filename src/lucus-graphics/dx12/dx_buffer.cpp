#include "dx_buffer.hpp"

using namespace lucus;

void dx_buffer::init(Com<ID3D12Device> device, uint32_t bufferSize)
{
    _device = device;

    // Create buffer

    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    uniformBuffersMapped.resize(g_framesInFlight);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    for (size_t i = 0; i < g_framesInFlight; ++i)
    {
        ThrowIfFailed(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uniformBuffers[i])), "Failed to create uniform buffer");

        // Map the buffers.
        ThrowIfFailed(uniformBuffers[i]->Map(0, nullptr, reinterpret_cast<void**>(&uniformBuffersMapped[i])), "Failed to map uniform buffer");
    }
}

void dx_buffer::cleanup()
{
    for (size_t i = 0; i < g_framesInFlight; ++i) {
        if (uniformBuffers[i]) {
            uniformBuffers[i]->Unmap(0, nullptr);
            uniformBuffers[i].Reset();
        }
    }
    uniformBuffersMapped.clear();
}

void dx_buffer::write(uint32_t index, const void* data, size_t size)
{
    if (index >= uniformBuffersMapped.size()) {
        throw std::out_of_range("Buffer index out of range");
    }
    memcpy(uniformBuffersMapped[index], data, size);
}