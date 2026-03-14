#pragma once

#include "vk_pch.hpp"

#include "renderer.hpp"
#include "vk_device.hpp"
#include "vk_swapchain.hpp"

namespace lucus
{
    class vk_renderer : public renderer
    {
        public:
            vk_renderer();
            ~vk_renderer() override = default;

            virtual bool init() override;
            virtual bool prepare(std::shared_ptr<window> window) override;
            virtual void tick() override;
            virtual void cleanup() override;
            
        protected:
            void createInstance();
            void initDevices();

            void createCommandBuffers();
            void destroyCommandBuffers();

            void createSyncObjects();
            void destroySyncObjects();

            void createRenderPass();

            void createGraphicsPipeline();

            VkShaderModule loadShader(const std::string& filepath) const;

            void createFramebuffers();

            void prepareFrame(bool waitFence = true);
            void buildCommandBuffer();
            void submitFrame(bool skipQueueSubmit = false);

        private:
            VkInstance _instance;

            // Physical device (GPU) that Vulkan will use
            VkPhysicalDevice _physicalDevice{ VK_NULL_HANDLE };
            // Stores physical device properties (for e.g. checking device limits)
            VkPhysicalDeviceProperties _deviceProperties{};
            // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
            VkPhysicalDeviceFeatures _deviceFeatures{};
            // Stores all available memory (type) properties for the physical device
            VkPhysicalDeviceMemoryProperties _deviceMemoryProperties{};
            
            // Logical device
            std::unique_ptr<vk_device> _device;
            VkDevice deviceVK;

            VkPhysicalDeviceFeatures _enabledFeatures{};
            std::vector<const char*> _enabledDeviceExtensions;
            std::vector<const char*> _enabledInstanceExtensions;

            VkQueue _graphicsQueue;

            //
            std::unique_ptr<vk_swapchain> _swapchain;

            // Command buffers used for rendering
	        std::array<VkCommandBuffer, g_maxConcurrentFrames> _drawCmdBuffers;

            // Synchronization related objects and variables
            // These are used to have multiple frame buffers "in flight" to get some CPU/GPU parallelism
            uint32_t _currentImageIndex{ 0 };
            uint32_t _currentBuffer{ 0 };
            std::array<VkSemaphore, g_maxConcurrentFrames> _presentCompleteSemaphores{};
            std::vector<VkSemaphore> _renderCompleteSemaphores{};
            std::array<VkFence, g_maxConcurrentFrames> _waitFences;

            VkRenderPass _renderPass;

            VkPipelineLayout _pipelineLayout;
            VkPipeline _graphicsPipeline;

            std::vector<VkFramebuffer> _frameBuffers;
    };
}