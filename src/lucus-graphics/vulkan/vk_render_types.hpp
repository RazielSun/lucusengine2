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

        u32 colorAttachmentCount{0};
        VkFormat colorFormats[g_maxMrtColorTargets];

        u32 depthAttachmentCount{0};
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

    struct vk_render_target_desc
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

    struct vk_render_target
    {
        bool bFromSwapChain { false };
        vk_render_target_desc desc;

        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        std::vector<VkDeviceMemory> memories;
        std::vector<VkDescriptorSet> descriptorSets;
        std::vector<resource_state> states;

        void init(VkDevice device, VkPhysicalDevice gpu, const vk_render_target_desc& init_desc);
        void createResources();
        void cleanup();

        private:
            VkDevice _device{ VK_NULL_HANDLE };
            VkPhysicalDevice _gpu{ VK_NULL_HANDLE };
    };

    struct vk_framebuffer_key
    {
        render_pass_name    pass;
        u8                  colorCount;
        render_target_handle colorTargets[g_maxMrtColorTargets];
        render_target_handle depthTarget;
        u32                 width;
        u32                 height;

        u64  hash() const;
    };

    struct vk_framebuffer_desc
    {
        u32 count { 0 };
        VkExtent2D extent;

        VkRenderPass renderPass;

        std::vector<std::vector<VkImageView>> viewsPerFrame;
    };

    struct vk_framebuffer
    {
        std::vector<VkFramebuffer> frameBuffers;

        void init(VkDevice device, VkPhysicalDevice gpu, const vk_framebuffer_desc& init_desc);
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
        /// max() if bindless slot was not allocated; otherwise index into bindless sampled-image array (see loadTextureToGPU).
        u32 bindlessSlot{std::numeric_limits<u32>::max()};

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
        bool bindless { false };
        VkDescriptorType type;
        shader_binding_stage stage;
        VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };

        void init(VkDevice device, VkDescriptorType in_type, shader_binding_stage in_stage, bool in_bindless = false);
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
