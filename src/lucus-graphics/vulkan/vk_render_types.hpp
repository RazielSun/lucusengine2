#pragma once

#include "vk_pch.hpp"

#include "vk_buffer.hpp"
#include "render_types.hpp"

namespace lucus
{
    class mesh;
    class texture;
    class material;

    struct vk_commandbuffer_pool
    {
        void init(VkDevice device, uint32_t queueFamilyIndex);
        void cleanup();
        
        VkCommandPool get() { return _commandPool; }
        VkCommandBuffer& getCommandBuffer(uint32_t index);
        
        protected:
            void allocateCommandBuffers();
            void destroyCommandBuffers();

        private:
            VkDevice _device;

            // A Command Pool is a data structure that allows you to create command buffers.
            VkCommandPool _commandPool{ VK_NULL_HANDLE };

            // Command buffers used for rendering
	        std::array<VkCommandBuffer, g_framesInFlight> _buffers;
    };

    struct vk_render_pass
    {
        VkFormat colorFormat{};
        VkFormat depthFormat{};

        VkRenderPass renderPass{ VK_NULL_HANDLE };

        void init(VkDevice device, VkFormat inColorFormat, VkFormat inDepthFormat);
        void cleanup();

        private:
            VkDevice _device;
    };

    struct vk_image
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;

        void init(VkDevice device);
        void cleanup();

        private:
            VkDevice _device{ VK_NULL_HANDLE };
    };

    struct vk_framebuffer
    {
        VkFramebuffer framebuffer{ VK_NULL_HANDLE };

        vk_image depth;

        void init(VkDevice device, VkPhysicalDevice gpu, VkRenderPass renderPass, VkExtent2D extent, VkImageView colorImageView, VkFormat depthFormat);
        void cleanup();

        private:
            VkDevice _device;
    };

    struct vk_viewport
    {
        VkViewport viewport{};
        VkRect2D scissor{};
    };

    struct vk_frame_sync
    {
        VkSemaphore imageAvailable {VK_NULL_HANDLE};
        VkSemaphore renderFinished {VK_NULL_HANDLE};
        VkFence fence {VK_NULL_HANDLE};

        void init(VkDevice device);
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_mesh : public rhi_mesh
    {
        vk_buffer vertexBuffer;
        vk_buffer indexBuffer;

        void init(VkDevice device, VkPhysicalDevice gpu, mesh* msh);
        void cleanup();
    };

    struct vk_texture
    {
        VkBuffer stgBuffer;
        VkDeviceMemory stgBufferMemory;
        VkImage texImage;
        VkDeviceMemory texImageMemory;
        VkImageView texImageView;
        VkSampler texSampler;
        VkDescriptorSet texDescriptorSet;
        VkDeviceSize texSize;
        VkExtent2D texExtent;

        void init(VkDevice device, VkPhysicalDevice gpu, texture* tex);
        void free_staging();
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_material : public rhi_material
    {
        VkDescriptorSet texDescriptorSet;

        void initDescriptor(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
        void addTexture(VkDevice device, uint32_t index, const vk_texture& tex);
        
        void cleanup();
    };
}