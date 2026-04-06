#pragma once

#include "vk_pch.hpp"

#include "vk_render_types.hpp"
#include "vk_buffer.hpp"

namespace lucus
{
    class window;

    struct vk_swapchain;

    struct vk_framebuffer_list
    {
        std::vector<vk_framebuffer> frameBuffers;

        void init(VkDevice device, VkPhysicalDevice gpu, VkRenderPass renderPass, const vk_swapchain& swapchain, VkFormat depthFormat);
        void cleanup();
    };

    struct vk_swapchain
    {
        VkFormat colorFormat{};
        VkColorSpaceKHR colorSpace{};
        VkExtent2D extent{};

        std::vector<VkImage> images{};
        std::vector<VkImageView> imageViews{};
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

        vk_render_pass renderPass;
        vk_framebuffer_list framebuffers;

        vk_viewport viewport;
        vk_uniform_buffer uniformbuffers;

        std::array<vk_frame_sync, g_framesInFlight> frames{};
        uint32_t currentFrame = 0;
        uint32_t currentImageIndex = 0;

        void init(VkInstance instance, VkPhysicalDevice gpu, VkDevice device, window* window);
        void cleanup();

        void wait_frame();

    protected:
        void initSurface(VkPhysicalDevice gpu, window* window);

    private:
        VkInstance _instance;
        VkDevice _device;
    };
}