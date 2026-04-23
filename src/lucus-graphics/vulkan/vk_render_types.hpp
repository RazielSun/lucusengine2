#pragma once

#include "vk_pch.hpp"

#include "vk_buffer.hpp"
#include "render_types.hpp"

namespace lucus
{
    class mesh;
    class texture;
    class material;

    struct vk_commandbuffer_pool
    {
        void init(VkDevice device, uint32_t queueFamilyIndex);
        void cleanup();
        
        VkCommandPool get() { return _commandPool; }
        VkCommandBuffer& getCommandBuffer(uint32_t index);
        
        protected:
            void allocateCommandBuffers();
            void destroyCommandBuffers();

        private:
            VkDevice _device;

            // A Command Pool is a data structure that allows you to create command buffers.
            VkCommandPool _commandPool{ VK_NULL_HANDLE };

            // Command buffers used for rendering
	        std::array<VkCommandBuffer, g_framesInFlight> _buffers;
    };

    struct vk_render_pass_desc
    {
        render_pass_name name;
        bool bUseColor{true};
        VkFormat colorFormat;
        bool bUseDepth{false};
        VkFormat depthFormat;
    };

    struct vk_render_pass
    {
        render_pass_name name;

        VkRenderPass renderPass{ VK_NULL_HANDLE };

        void init(VkDevice device, const vk_render_pass_desc& init_desc);
        void cleanup();

        private:
            VkDevice _device;
    };

    struct vk_images_desc
    {
        u32 count {0};
        VkFormat format;
        VkExtent2D extent;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags properties;
        VkImageAspectFlags aspectFlags;
        bool bUseDescriptorSet { false };
        VkDescriptorPool descriptorPool { VK_NULL_HANDLE };
        VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };
    };

    struct vk_images
    {
        bool bPreinitialized { false };
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        std::vector<VkDeviceMemory> memories;
        std::vector<VkDescriptorSet> descriptorSets;

        void init(VkDevice device, VkPhysicalDevice gpu, const vk_images_desc& init_desc);
        void cleanup();

        private:
            VkDevice _device{ VK_NULL_HANDLE };
    };

    struct vk_render_target_desc
    {
        u32 count { 0 };
        VkExtent2D extent;

        bool bUseColor {false};
        bool bUseColorDescriptorSet { false };
        VkFormat colorFormat;

        bool bUseDepth {false};
        bool bUseDepthDescriptorSet { false };
        VkFormat depthFormat;

        bool bUseFramebuffer{false};
        VkRenderPass renderPass;

        VkDescriptorPool descriptorPool { VK_NULL_HANDLE };
        VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };
    };

    struct vk_render_target
    {
        bool bColor { true };
        vk_images color;

        bool bDepth { false };
        vk_images depth;

        bool bFramebuffer { false };
        bool bSwapChain { false };
        std::vector<VkFramebuffer> frameBuffers;

        void init(VkDevice device, VkPhysicalDevice gpu, const vk_render_target_desc& init_desc);
        void cleanup();

        private:
            VkDevice _device;
    };

    struct vk_image_sync
    {
        VkSemaphore imageAvailable {VK_NULL_HANDLE};
        VkSemaphore renderFinished {VK_NULL_HANDLE};

        void init(VkDevice device);
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_frame_sync
    {
        VkFence fence {VK_NULL_HANDLE};

        void init(VkDevice device);
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_mesh : public gpu_mesh
    {
        vk_buffer vertexBuffer;
        vk_buffer indexBuffer;

        void init(VkDevice device, VkPhysicalDevice gpu, mesh* msh);
        void cleanup();
    };

    struct vk_texture
    {
        VkBuffer stgBuffer;
        VkDeviceMemory stgBufferMemory;

        VkImage texImage;
        VkDeviceMemory texImageMemory;
        VkImageView texImageView;
        VkDescriptorSet descriptorSet;

        VkDeviceSize texSize;
        VkExtent2D texExtent;

        void init(texture* tex, VkDevice device, VkPhysicalDevice gpu, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
        void free_staging();
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_sampler
    {
        VkSampler sampler;
        VkDescriptorSet descriptorSet;

        void init(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_descriptor
    {
        VkDescriptorType type;
        shader_binding_stage stage;
        VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };

        void init(VkDevice device, VkDescriptorType in_type, shader_binding_stage in_stage);
        void cleanup();

    private:
        VkDevice _device;
    };

    struct vk_barrier_type
    {
        VkImageLayout layout;
        VkAccessFlags access;
        VkPipelineStageFlags stage;
    };
}
