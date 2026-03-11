#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    class window;

    class iswapchain
    {
        public:
            virtual ~iswapchain() = default;

            virtual void createSurface(std::shared_ptr<window> window) = 0;
            virtual void create(std::shared_ptr<window> window) = 0;
    };

    class vk_swapchain : public iswapchain
    {
        public:
            vk_swapchain() = default;
            virtual ~vk_swapchain();

            void setContext(VkInstance instance, VkPhysicalDevice gpu, VkDevice device);

            virtual void createSurface(std::shared_ptr<window> window) override;
            virtual void create(std::shared_ptr<window> window) override;

            VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex);

            VkFormat colorFormat{};
            VkColorSpaceKHR colorSpace{};
            VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
            VkExtent2D extent{};
            std::vector<VkImage> images{};
            std::vector<VkImageView> imageViews{};
            uint32_t queueNodeIndex{ UINT32_MAX };
            uint32_t imageCount{ 0 };

        private:
            VkInstance _instance;
            VkPhysicalDevice _gpu;
            VkDevice _device;

            VkSurfaceKHR _surface;
    };
}