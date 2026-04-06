#pragma once

#include "vk_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class vk_buffer
    {
        public:
            void init(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
            void cleanup();

            void map();
            void unmap();
            void write(const void* data, size_t size, size_t offset = 0);

            VkBuffer get() const { return _buffer; }
            VkDeviceMemory getMemory() const { return _memory; }
            VkDeviceSize getSize() const { return _size; }

        private:
            VkDevice _device{ VK_NULL_HANDLE };
            VkPhysicalDevice _gpu{ VK_NULL_HANDLE };
            VkBuffer _buffer{ VK_NULL_HANDLE };
            VkDeviceMemory _memory{ VK_NULL_HANDLE };
            void* _mapped{ nullptr };
            VkDeviceSize _size{ 0 };
    };

    class vk_uniform_buffer
    {
        public:
            void init(VkDevice device, VkPhysicalDevice gpu, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, VkDeviceSize bufferSize);
            void cleanup();

            void write(uint32_t index, const void* data, size_t size);

            VkDescriptorSet* get(uint32_t index) { return &_descriptorSets[index]; }

        protected:
            void createDescriptorSets(VkDeviceSize bufferSize);

        private:
            VkDevice _device;
            VkPhysicalDevice _gpu;

            VkDescriptorSetLayout _descriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorPool _descriptorPool{ VK_NULL_HANDLE };

            std::array<VkDescriptorSet, g_framesInFlight> _descriptorSets;

            std::array<vk_buffer, g_framesInFlight> _buffers;
    };

    class mesh;

    struct vk_mesh
    {
        bool bHasVertexData{false};
        uint32_t vertexCount{0};
        uint32_t indexCount{0};

        vk_buffer vertexBuffer;
        vk_buffer indexBuffer;

        void init(VkDevice device, VkPhysicalDevice gpu, mesh* msh);
        void cleanup();
    };
}