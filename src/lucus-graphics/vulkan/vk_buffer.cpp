#include "vk_buffer.hpp"

#include "vk_utils.hpp"

using namespace lucus;

void vk_buffer::init(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    _device = device;
    _gpu = gpu;
    _rawSize = size;
    _size = (size + 255) & ~255;

    utils::createBuffer(_device, _gpu, _size, usage, properties, _buffer, _memory);
}

void vk_buffer::cleanup()
{
    if (_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(_device, _buffer, nullptr);
        _buffer = VK_NULL_HANDLE;
    }
    if (_memory != VK_NULL_HANDLE) {
        vkFreeMemory(_device, _memory, nullptr);
        _memory = VK_NULL_HANDLE;
    }
}

void vk_buffer::map()
{
    if (vkMapMemory(_device, _memory, 0, _size, 0, &_mapped) != VK_SUCCESS) {
        throw std::runtime_error("failed to map buffer memory!");
    }
}

void vk_buffer::unmap()
{
    if (_mapped) {
        vkUnmapMemory(_device, _memory);
        _mapped = nullptr;
    }
}

void vk_buffer::write(const void* data, size_t size, size_t offset)
{
    if (!_mapped) {
        throw std::runtime_error("Buffer memory is not mapped!");
    }
    if (offset + size > _size) {
        throw std::out_of_range("Write range exceeds buffer size!");
    }
    std::memcpy(static_cast<char*>(_mapped) + offset, data, size);
}

void vk_uniform_buffer::init(VkDevice device, VkPhysicalDevice gpu, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, VkDeviceSize bufferSize)
{
    _device = device;
    _gpu = gpu;
    _descriptorSetLayout = descriptorSetLayout;
    _descriptorPool = descriptorPool;

    for (size_t i = 0; i < g_framesInFlight; i++)
    {
        _buffers[i].init(_device, _gpu, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        _buffers[i].map();
    }

    createDescriptorSets(bufferSize);
}

void vk_uniform_buffer::cleanup()
{
    for (size_t i = 0; i < g_framesInFlight; i++)
    {
        _buffers[i].cleanup();
    }
}

void vk_uniform_buffer::write(uint32_t index, const void* data, size_t size)
{
    if (index >= _buffers.size()) {
        throw std::out_of_range("Buffer index out of range");
    }
    _buffers[index].write(data, size);
}

void vk_uniform_buffer::createDescriptorSets(VkDeviceSize bufferSize)
{
    std::vector<VkDescriptorSetLayout> layouts(g_framesInFlight, _descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(g_framesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(_device, &allocInfo, _descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < g_framesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = _buffers[i].get();
        bufferInfo.offset = 0;
        bufferInfo.range = bufferSize;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = _descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        vkUpdateDescriptorSets(_device, 1, &descriptorWrite, 0, nullptr);
    }
}