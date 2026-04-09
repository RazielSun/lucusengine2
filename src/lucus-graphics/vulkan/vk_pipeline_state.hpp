#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    class material;

    class vk_pipeline_state
    {
    public:
        vk_pipeline_state(VkDevice device);
        ~vk_pipeline_state();

        void init(const material* mat, VkRenderPass renderPass, uint32_t layoutCount = 0, VkDescriptorSetLayout* layouts = nullptr);

        VkPipeline& getPipeline() { return _pipeline; }
        VkPipelineLayout& getPipelineLayout() { return _pipelineLayout; }
    
    protected:
        VkShaderModule loadShader(const std::string& filepath) const;

        void createPipelineLayout(uint32_t layoutCount = 0, VkDescriptorSetLayout* layouts = nullptr);

    private:
        VkDevice _device;

        VkPipeline _pipeline{ VK_NULL_HANDLE };
        VkPipelineLayout _pipelineLayout{ VK_NULL_HANDLE };
    };
}