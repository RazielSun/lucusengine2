#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    class vk_pipeline_state
    {
        public:
            vk_pipeline_state(VkDevice device);
            ~vk_pipeline_state();

            void init(const std::string& shaderName, VkRenderPass renderPass);

            VkPipeline& getPipeline() { return _pipeline; }
            VkPipelineLayout& getPipelineLayout() { return _pipelineLayout; }
        
        protected:
            VkShaderModule loadShader(const std::string& filepath) const;

        private:
            VkDevice _device;

            VkPipeline _pipeline{ VK_NULL_HANDLE };
            VkPipelineLayout _pipelineLayout{ VK_NULL_HANDLE };
    };
}