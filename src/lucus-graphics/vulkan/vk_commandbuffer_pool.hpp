#pragma once

#include "vk_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class vk_commandbuffer_pool
    {
        public:
            vk_commandbuffer_pool(VkDevice device);
            ~vk_commandbuffer_pool();

            void initCommandPool(uint32_t queueFamilyIndex);

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
}