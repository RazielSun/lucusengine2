#pragma once

#include "vk_pch.hpp"

#include "vk_viewport.hpp"

namespace lucus
{
    struct vk_render_pass
    {
        void init(VkDevice device, VkFormat format);
        void cleanup(VkDevice device);

        VkFormat colorFormat{};
        VkRenderPass renderPass{ VK_NULL_HANDLE };
    };

    struct vk_framebuffer
    {
        void init(VkDevice device, const vk_viewport& viewport, VkRenderPass renderPass);
        void cleanup(VkDevice device);

        std::vector<VkFramebuffer> frameBuffers;
    };

    struct vk_commandbuffer_pool
    {
        public:
            void init(VkDevice device, uint32_t queueFamilyIndex);
            void cleanup(VkDevice device);

            VkCommandBuffer& getCommandBuffer(uint32_t index);
        
        protected:
            void allocateCommandBuffers(VkDevice device);
            void destroyCommandBuffers(VkDevice device);

        private:
            // A Command Pool is a data structure that allows you to create command buffers.
            VkCommandPool _commandPool{ VK_NULL_HANDLE };

            // Command buffers used for rendering
	        std::array<VkCommandBuffer, g_framesInFlight> _buffers;
    };
}