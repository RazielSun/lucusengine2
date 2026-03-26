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

            void init(material* mat, VkRenderPass renderPass, VkDescriptorSetLayout* dscSetLayout = nullptr);

            VkPipeline& getPipeline() { return _pipeline; }
            VkPipelineLayout& getPipelineLayout() { return _pipelineLayout; }
            bool isUniformBufferUsed() const { return _useUniformBuffers; }
        
        protected:
            VkShaderModule loadShader(const std::string& filepath) const;

            void createPipelineLayout(VkDescriptorSetLayout* dscSetLayout = nullptr);

        private:
            VkDevice _device;

            VkPipeline _pipeline{ VK_NULL_HANDLE };
            VkPipelineLayout _pipelineLayout{ VK_NULL_HANDLE };

            bool _useUniformBuffers{false};
    };
}