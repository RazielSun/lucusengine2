#pragma once

#include "vk_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class vk_buffer
    {
        public:
            void init(VkDevice device, VkPhysicalDevice gpu, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, VkDeviceSize bufferSize);
            void cleanup();

            void write(uint32_t index, const void* data, size_t size);

            VkDescriptorSet* getDescriptorSet(uint32_t index) { return &_descriptorSets[index]; }

        protected:
            void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
            uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

            void createDescriptorSets(VkDeviceSize bufferSize);

        private:
            VkDevice _device;
            VkPhysicalDevice _gpu;

            VkDescriptorSetLayout _descriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorPool _descriptorPool{ VK_NULL_HANDLE };

            std::array<VkBuffer, g_framesInFlight> uniformBuffers;
            std::array<VkDeviceMemory, g_framesInFlight> uniformBuffersMemory;
            std::vector<void*> uniformBuffersMapped;

            std::array<VkDescriptorSet, g_framesInFlight> _descriptorSets;
    };
}