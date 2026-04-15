#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    class material;

    struct vk_pipeline_state_desc
    {
        VkRenderPass renderPass;

        std::vector<VkDescriptorSetLayout> layouts;

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
        VkPipelineLayout& getPipelineLayout() { return _pipelineLayout; }
    
    protected:
        VkShaderModule loadShader(const std::string& filepath) const;

        void createPipelineLayout(uint32_t layoutCount = 0, const VkDescriptorSetLayout* layouts = nullptr);

    private:
        VkDevice _device;

        VkPipeline _pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout _pipelineLayout{ VK_NULL_HANDLE };
    };
}