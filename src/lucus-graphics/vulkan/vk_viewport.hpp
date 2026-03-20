#pragma once

#include "vk_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class window;
    
    struct vk_viewport
    {
        VkSurfaceKHR surface{ VK_NULL_HANDLE };
        
        VkFormat colorFormat{};
        VkColorSpaceKHR colorSpace{};
        VkExtent2D extent{};

        std::vector<VkImage> images{};
        std::vector<VkImageView> imageViews{};
        uint32_t imageCount{ 0 };

        VkSwapchainKHR swapChain{ VK_NULL_HANDLE };

        VkViewport viewport{};
        VkRect2D scissor{};

        void init(VkInstance instance, VkPhysicalDevice gpu, VkDevice device, window* window);
        void cleanup(VkInstance instance, VkDevice device);

    protected:
        void initSurface(VkPhysicalDevice gpu, window* window);
        void createSwapchain(VkPhysicalDevice gpu, VkDevice device, window* window);
    };
}