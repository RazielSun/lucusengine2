#pragma once

#include "vk_pch.hpp"

#include "dynamic_rhi.hpp"

#include "vk_device.hpp"
#include "vk_buffer.hpp"
#include "vk_render_types.hpp"
#include "vk_window_context.hpp"

namespace lucus
{
    class vk_pipeline_state;

    class vk_dynamic_rhi : public dynamic_rhi
    {
        public:
            vk_dynamic_rhi();
            virtual ~vk_dynamic_rhi() override;

            virtual void init() override;

            virtual window_context_handle createWindowContext(const window_handle& handle) override;
            virtual const std::vector<window_context_handle>& getWindowContexts() const override;
            virtual float getWindowContextAspectRatio(const window_context_handle& handle) const override;

            virtual void beginFrame(const window_context_handle& handle) override;
            virtual void submit(const window_context_handle& handle, const command_buffer& cmd) override;
            virtual void endFrame(const window_context_handle& handle) override;

            virtual material_handle createMaterial(material* mat) override;

            virtual render_object_handle createUniformBuffer(render_object* obj) override;

        protected:
            void createInstance();
            void createDevice();
            void createCommandBufferPool();
            
            void createDescriptorPool();
            void createDescriptorSetLayouts();
            void createFrameUniformBuffers();

            void wait_idle();

        private:
            VkInstance _instance;

            std::unique_ptr<vk_device> _device;
            VkDevice _deviceHandle;

            VkPhysicalDeviceFeatures _enabledFeatures{};
            std::vector<const char*> _enabledDeviceExtensions;
            std::vector<const char*> _enabledInstanceExtensions;

            std::vector<vk_window_context> _contexts;
            std::vector<window_context_handle> _contextHandles;

            vk_commandbuffer_pool _commandbuffer_pool;

            //
            std::unordered_map<uint32_t, vk_pipeline_state> _pipelineStates;

            //
            VkQueue _graphicsQueue;

            VkDescriptorSetLayout _frameDescriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorSetLayout _objectDescriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorPool _descriptorPool{ VK_NULL_HANDLE };

            vk_buffer _frameUniformBuffer;
            std::vector<vk_buffer> _objectUniformBuffers;
    };
}