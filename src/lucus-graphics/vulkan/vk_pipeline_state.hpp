#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    class material;

    struct vk_pipeline_layout
    {
        void init(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts);
        void cleanup();

        VkPipelineLayout get() const { return _layout; }
        bool is_valid() const { return _layout != VK_NULL_HANDLE; }

    private:
        VkDevice _device{ VK_NULL_HANDLE };
        VkPipelineLayout _layout{ VK_NULL_HANDLE };
    };

    struct vk_pipeline_state_desc
    {
        VkRenderPass renderPass;
        uint32_t colorAttachmentsCount {0};

        VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

        VkVertexInputBindingDescription bindingDesc{};

        std::vector<VkVertexInputAttributeDescription> attributes;
    };

    class vk_pipeline_state
    {
    public:
        vk_pipeline_state(VkDevice device);
        ~vk_pipeline_state();

        void init(const std::string& shaderName, const vk_pipeline_state_desc& init_desc);

        VkPipeline& getPipeline() { return _pipeline; }

    protected:
        VkShaderModule loadShader(const std::string& filepath) const;

    private:
        VkDevice _device;

        VkPipeline _pipeline{ VK_NULL_HANDLE };
    };
}
