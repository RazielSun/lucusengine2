#include "vk_commandbuffer_pool.hpp"

using namespace lucus;

vk_commandbuffer_pool::vk_commandbuffer_pool(VkDevice device) : _device(device)
{
    //
}

vk_commandbuffer_pool::~vk_commandbuffer_pool()
{
    destroyCommandBuffers();
    if (_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(_device, _commandPool, nullptr);
    }
}

void vk_commandbuffer_pool::initCommandPool(uint32_t queueFamilyIndex)
{
    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    if (vkCreateCommandPool(_device, &cmdPoolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    allocateCommandBuffers();
}

VkCommandBuffer& vk_commandbuffer_pool::getCommandBuffer(uint32_t index)
{
    return _buffers[index];
}

void vk_commandbuffer_pool::allocateCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(_buffers.size()),
    };

    if (vkAllocateCommandBuffers(_device, &allocInfo, _buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    std::printf("Command buffer created successfully\n");
}

void vk_commandbuffer_pool::destroyCommandBuffers()
{
    vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_buffers.size()), _buffers.data());
}