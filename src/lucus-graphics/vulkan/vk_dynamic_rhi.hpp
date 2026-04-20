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
            virtual void getWindowContextSize(const window_context_handle& handle, u32& width, u32& height) const override;

            virtual pipeline_state_handle createPSO(material* mat) override;
            virtual mesh_handle createMesh(mesh* msh) override;

            virtual sampler_handle createSampler() override;
            virtual texture_handle createTexture(texture* tex) override;
            virtual void loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex) override;

            virtual uniform_buffer_handle createUniformBuffer(size_t bufferSize, shader_binding_stage stage = shader_binding_stage::VERTEX) override;
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

            VkDescriptorSetLayout findDescriptor(VkDescriptorType in_type, shader_binding_stage in_stage) const;

        private:
            VkInstance _instance;

            std::unique_ptr<vk_device> _device;
            VkDevice _deviceHandle;

            VkPhysicalDeviceFeatures _enabledFeatures{};
            std::vector<const char*> _enabledDeviceExtensions;
            std::vector<const char*> _enabledInstanceExtensions;

            std::vector<vk_window_context> _contexts;

            // TODO: refactor me
            vk_render_pass mainRenderPass;

            vk_commandbuffer_pool _commandbuffer_pool;

            VkQueue _graphicsQueue;

            //
            std::unordered_map<u32, vk_pipeline_state> _pipelineStates;

            //
            std::unordered_map<u32, vk_mesh> _meshes;

            //
            std::vector<vk_sampler> _samplers;
            std::unordered_map<u32, vk_texture> _textures;

            //
            std::vector<vk_uniform_buffer> _uniformBuffers;

            //
            std::vector<vk_descriptor> _descriptors;
            VkDescriptorPool _descriptorPool{ VK_NULL_HANDLE };

            //
            std::array<vk_frame_sync, g_framesInFlight> frameSync{};
    };
}