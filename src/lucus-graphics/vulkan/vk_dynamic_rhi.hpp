#pragma once

#include "vk_pch.hpp"

#include "dynamic_rhi.hpp"

#include "vk_render_types.hpp"
#include "vk_device.hpp"
#include "vk_viewport.hpp"

namespace lucus
{
    class vk_pipeline_state;

    class vk_dynamic_rhi : public dynamic_rhi
    {
        public:
            vk_dynamic_rhi();
            virtual ~vk_dynamic_rhi() override;

            virtual void init() override;

            virtual viewport_handle createViewport(const window_handle& handle) override;

            virtual void beginFrame(const viewport_handle& handle) override;
            virtual void endFrame() override;

            virtual void submit(const command_buffer& cmd) override;

            virtual material_handle createMaterial(material* mat) override;

        protected:
            void createInstance();
            void createDevice();
            void createCommandBufferPool();

            void createSyncObjectsStable();
            void createSyncObjectsBy(int imageCount);

            void wait_idle();

            void getOrCreateRenderPassAndFramebuffer(const vk_viewport& viewport, vk_render_pass& outRenderPass, vk_framebuffer& outFramebuffer);
            void createFramebuffer(const vk_viewport& viewport, const vk_render_pass& renderPass);
        
        private:
            VkInstance _instance;

            std::unique_ptr<vk_device> _device;
            VkDevice _deviceHandle;

            VkPhysicalDeviceFeatures _enabledFeatures{};
            std::vector<const char*> _enabledDeviceExtensions;
            std::vector<const char*> _enabledInstanceExtensions;

            std::vector<vk_viewport> _viewports;

            vk_commandbuffer_pool _commandbuffer_pool;

            viewport_handle _currentViewport;
            uint32_t _currentBufferIndex{ 0 };
            uint32_t _currentImageIndex{ 0 };

            //
            std::unordered_map<uint32_t, vk_pipeline_state> _pipelineStates;

            //
            std::array<VkSemaphore, g_framesInFlight> _presentCompleteSemaphores{};
            std::vector<VkSemaphore> _renderCompleteSemaphores{};
            std::array<VkFence, g_framesInFlight> _waitFences{};

            //
            VkQueue _graphicsQueue;

            std::vector<vk_render_pass> _renderPasses;
            std::vector<vk_framebuffer> _frameBuffers;
    };
}