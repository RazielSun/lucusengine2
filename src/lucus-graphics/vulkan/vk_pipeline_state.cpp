#include "vk_pipeline_state.hpp"

#include "filesystem.hpp"
#include "material.hpp"

using namespace lucus;

void vk_pipeline_layout::init(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts)
{
    cleanup();
    _device = device;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pipelineLayoutInfo.pSetLayouts = layouts.empty() ? nullptr : layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void vk_pipeline_layout::cleanup()
{
    if (_layout != VK_NULL_HANDLE && _device != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_device, _layout, nullptr);
    }
    _layout = VK_NULL_HANDLE;
    _device = VK_NULL_HANDLE;
}

vk_pipeline_state::vk_pipeline_state(VkDevice device)
    : _device(device)
{
}

vk_pipeline_state::~vk_pipeline_state()
{
    if (_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(_device, _pipeline, nullptr);
    }
}

void vk_pipeline_state::init(const std::string& shaderName, const vk_pipeline_state_desc& init_desc)
{
    VkShaderModule vertShaderModule = loadShader(shaderName + ".vert.spv");
    VkShaderModule fragShaderModule = loadShader(shaderName + ".frag.spv");

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // Optional, because we are using dynamic state
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // Optional, because we are using dynamic state

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    if ((u32)init_desc.attributes.size() > 0)
    {
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &init_desc.bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<u32>(init_desc.attributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = init_desc.attributes.data();
    }
    else
    {
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(init_desc.colorAttachmentsCount);
    for (u32 i = 0; i < init_desc.colorAttachmentsCount; ++i)
    {
        colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachments[i].blendEnable = VK_FALSE;
        colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
    } 

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = init_desc.colorAttachmentsCount;
    colorBlending.pAttachments = colorBlendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    if (init_desc.pipelineLayout == VK_NULL_HANDLE) {
        throw std::runtime_error("vk_pipeline_state::init: pipelineLayout is null");
    }

    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;

    depthState.depthBoundsTestEnable = VK_FALSE;
    depthState.minDepthBounds = 0.0f;
    depthState.maxDepthBounds = 1.0f;

    depthState.stencilTestEnable = VK_FALSE;
    depthState.front = {};
    depthState.back = {};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = init_desc.pipelineLayout;
    pipelineInfo.renderPass = init_desc.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(_device, vertShaderModule, nullptr);

    std::printf("Graphics pipeline created successfully\n");
}

VkShaderModule vk_pipeline_state::loadShader(const std::string& filepath) const
{
    auto shaderCode = filesystem::instance().read_shader(filepath);

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderCode.size(),
        .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

