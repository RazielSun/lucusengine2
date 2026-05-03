#pragma once

#include "vk_pch.hpp"

#include "vk_render_types.hpp"
#include "vk_buffer.hpp"

namespace lucus
{
    class window;

    struct vk_swapchain
    {
        VkFormat colorFormat{};
        VkColorSpaceKHR colorSpace{};
        VkExtent2D extent{};

        uint32_t imageCount{ 0 };

        VkSwapchainKHR swapChain{ VK_NULL_HANDLE };

        void init(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface, window* window);
        void cleanup();

        private:
            VkDevice _device;
    };
    
    struct vk_window_context
    {
        VkSurfaceKHR surface{ VK_NULL_HANDLE };

        vk_swapchain swapChain;

        VkSurfaceFormatKHR selectedFormat;

        uint32_t currentImageIndex = 0;
        std::array<vk_image_sync, g_swapchainImageCount> imageSync{};

        render_target_handle color_handle;
        render_target_handle depth_handle;

        window_gbuffer_targets gbuffer{};

        void init(VkInstance instance, VkPhysicalDevice gpu, VkDevice device, window* window);
        void init_images(vk_render_target& render_target);
        void cleanup();

        void acquire_image(u32 frameIndex);

    protected:
        void initSurface(VkPhysicalDevice gpu, window* window);

    private:
        VkInstance _instance;
        VkDevice _device;
        VkPhysicalDevice _gpu;
    };
}