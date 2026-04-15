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
            virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const override;

            virtual pipeline_state_handle createPSO(material* mat) override;
            virtual mesh_handle createMesh(mesh* msh) override;
            virtual texture_handle loadTexture(texture* tex) override;

            virtual uniform_buffer_handle createUniformBuffer(uniform_buffer_type ub_type, size_t bufferSize) override;
            virtual void getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr) override;

            virtual void execute(const window_context_handle& handle, u32 frameIndex, const gpu_command_buffer& cmd) override;

        protected:
            void createInstance();
            void createDevice();
            void createCommandBufferPool();
            
            void createDescriptorPool();
            void createDescriptorSetLayouts();

            void wait_idle();

            VkCommandBuffer beginSingleTimeCommands();
            void endSingleTimeCommands(VkCommandBuffer commandBuffer);

            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
            void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
            void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

        private:
            VkInstance _instance;

            std::unique_ptr<vk_device> _device;
            VkDevice _deviceHandle;

            VkPhysicalDeviceFeatures _enabledFeatures{};
            std::vector<const char*> _enabledDeviceExtensions;
            std::vector<const char*> _enabledInstanceExtensions;

            std::vector<vk_window_context> _contexts;
            std::vector<window_context_handle> _contextHandles;

            // TODO: refactor me
            vk_render_pass mainRenderPass;

            vk_commandbuffer_pool _commandbuffer_pool;

            VkQueue _graphicsQueue;

            //
            std::unordered_map<u32, vk_pipeline_state> _pipelineStates;

            //
            std::unordered_map<u32, vk_mesh> _meshes;

            //
            std::unordered_map<u32, vk_texture> _textures;

            //
            std::vector<vk_uniform_buffer> _uniformBuffers;
            std::unordered_map<uniform_buffer_type, VkDescriptorSetLayout> _ubDescriptorSetLayouts;

            // VkDescriptorSetLayout _frameDescriptorSetLayout{ VK_NULL_HANDLE };
            // VkDescriptorSetLayout _objectDescriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorSetLayout _texturesDescriptorSetLayout{ VK_NULL_HANDLE };
            VkDescriptorPool _descriptorPool{ VK_NULL_HANDLE };
    };
}