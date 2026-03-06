#pragma once

#include "pch.hpp"

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

namespace lucus
{
    class window;

    class renderer
    {
    public:
        bool init(std::shared_ptr<window> window);
        void mainLoop();
        void cleanup();
        
    protected:

        bool createInstance();

        // TODO: Debug Layers from Tutorial

        bool createSurface(std::shared_ptr<window> window);

        void pickPhysicalDevice();

        void createLogicalDevice();

        void createSwapChain(std::shared_ptr<window> window);

        void createImageViews();

        void createRenderPass();

        void createGraphicsPipeline();

        void createFramebuffers();

        void createCommandPool();
        void createCommandBuffer();

        void createSyncObjects();

        // RUNTIME DRAW
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void drawFrame();

    private:
        VkInstance _instance;

        VkSurfaceKHR _surface;

        VkPhysicalDevice _physicalDevice;

        VkDevice _device;

        VkQueue _graphicsQueue;
        VkQueue _presentQueue;

        VkSwapchainKHR _swapChain;
        std::vector<VkImage> _swapChainImages;
        VkFormat _swapChainImageFormat;
        VkExtent2D _swapChainExtent;

        std::vector<VkImageView> _swapChainImageViews;

        VkRenderPass _renderPass;
        VkPipelineLayout _pipelineLayout;
        VkPipeline _graphicsPipeline;

        std::vector<VkFramebuffer> _swapChainFramebuffers;

        VkCommandPool _commandPool;
        VkCommandBuffer _commandBuffer;

        VkSemaphore _imageAvailableSemaphore;
        VkSemaphore _renderFinishedSemaphore;
        VkFence _inFlightFence;
    };
} // namespace lucus