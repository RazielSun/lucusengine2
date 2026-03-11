#include "vk_renderer.hpp"

#include "filesystem.hpp"
#include "application_info.hpp"
#include "engine_info.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

std::shared_ptr<lucus::renderer> lucus::create_renderer()
{
    auto renderer = std::make_shared<lucus::vk_renderer>();
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
}

using namespace lucus;

vk_renderer::vk_renderer()
{
    _swapchain = std::make_unique<vk_swapchain>();
}

bool vk_renderer::init()
{
    createInstance();
    initDevices();
    return true;
}

bool vk_renderer::prepare(std::shared_ptr<window> window)
{
    _swapchain->createSurface(window);
    _swapchain->create(window);
    createCommandBuffers();
    createSyncObjects();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    return true;
}

void vk_renderer::tick()
{
    prepareFrame();
    buildCommandBuffer();
    submitFrame();

    vkDeviceWaitIdle(deviceVK);
}

void vk_renderer::cleanup()
{
    for (auto framebuffer : _frameBuffers) {
        vkDestroyFramebuffer(deviceVK, framebuffer, nullptr);
    }
    vkDestroyPipeline(deviceVK, _graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(deviceVK, _pipelineLayout, nullptr);
    vkDestroyRenderPass(deviceVK, _renderPass, nullptr);

    destroySyncObjects();
    destroyCommandBuffers();
    
    _swapchain.reset();
    _device.reset();
    vkDestroyInstance(_instance, nullptr);
}

void vk_renderer::createInstance()
{
    application_info& app_info = application_info::instance();
    engine_info& engine_info = engine_info::instance();

    VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = app_info.app_name().c_str(),
		.pEngineName = engine_info.name().c_str(),
		.apiVersion = VK_API_VERSION_1_0
	};

    VkInstanceCreateInfo instanceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo
	};

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::printf("RequiredInstanceExtensions = %d\n", glfwExtensionCount);
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        std::printf("  %s\n", glfwExtensions[i]);
    }

    instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;

    #ifdef NDEBUG
    const bool enableValidationLayers = false;
    #else
    const bool enableValidationLayers = true;
    #endif

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
	// Note that on Android this layer requires at least NDK r20
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	if (enableValidationLayers) {
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		for (VkLayerProperties& layer : instanceLayerProperties) {
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		} else {
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}

    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    std::printf("VkInstance created successfully\n");
}

void vk_renderer::initDevices()
{
    // Physical device
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(_instance, &gpuCount, nullptr);
    if (gpuCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    if (vkEnumeratePhysicalDevices(_instance, &gpuCount, physicalDevices.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to enumerate physical devices!");
    }

    // GPU selection

    uint32_t selectedDevice = 0;

    // TODO: Device suitability checks

    // for (const auto& device : physicalDevices) {
    //     if (isDeviceSuitable(device, _surface)) {
    //         _physicalDevice = device;
    //         break;
    //     }
    // }

    _physicalDevice = physicalDevices[selectedDevice];

    // getEnabledFeatures();
    // getEnabledExtensions();

    // Vulkan device creation
    _device = std::make_unique<vk_device>(_physicalDevice);

    //
    _device->createLogicalDevice(_enabledFeatures, _enabledDeviceExtensions);

    deviceVK = _device->getDevice();

    // Get a graphics queue from the device
    vkGetDeviceQueue(deviceVK, _device->queueFamilyIndices.graphics, 0, &_graphicsQueue);
    // vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);

    _swapchain->setContext(_instance, _physicalDevice, deviceVK);
}

void vk_renderer::createCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _device->getCommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(_drawCmdBuffers.size()),
    };

    if (vkAllocateCommandBuffers(deviceVK, &allocInfo, _drawCmdBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    std::printf("Command buffer created successfully\n");
}

void vk_renderer::destroyCommandBuffers()
{
	vkFreeCommandBuffers(deviceVK, _device->getCommandPool(), static_cast<uint32_t>(_drawCmdBuffers.size()), _drawCmdBuffers.data());
}

void vk_renderer::createSyncObjects()
{
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    // Wait fences to sync command buffer access
    for (auto& fence : _waitFences) {
        if (vkCreateFence(deviceVK, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    // Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	_renderCompleteSemaphores.resize(_swapchain->images.size());
    for (auto& semaphore : _renderCompleteSemaphores) {
        if (vkCreateSemaphore(deviceVK, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    // Used to ensure that image presentation is complete before starting to submit again
    for (auto& semaphore : _presentCompleteSemaphores) {
        if (vkCreateSemaphore(deviceVK, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    std::printf("Semaphores and fences created successfully\n");
}

void vk_renderer::destroySyncObjects()
{
    for (auto& fence : _waitFences) {
        vkDestroyFence(deviceVK, fence, nullptr);
    }

    for (auto& semaphore : _renderCompleteSemaphores) {
        vkDestroySemaphore(deviceVK, semaphore, nullptr);
    }

    for (auto& semaphore : _presentCompleteSemaphores) {
        vkDestroySemaphore(deviceVK, semaphore, nullptr);
    }
}


void vk_renderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapchain->colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(deviceVK, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }

    std::printf("Render pass created successfully\n");
}


void vk_renderer::createGraphicsPipeline()
{
    VkShaderModule vertShaderModule = loadShader("shaders/vert.spv");
    VkShaderModule fragShaderModule = loadShader("shaders/frag.spv");

    // Pipeline
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

    //
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Viewports and scissors
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)_swapchain->extent.width;
    viewport.height = (float)_swapchain->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _swapchain->extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Because we are hard coding the vertex data directly
    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    // Depth and stencil testing
    // We don't have one right now

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(deviceVK, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(deviceVK, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(deviceVK, fragShaderModule, nullptr);
    vkDestroyShaderModule(deviceVK, vertShaderModule, nullptr);

    std::printf("Graphics pipeline created successfully\n");
}

VkShaderModule vk_renderer::loadShader(const std::string& filepath) const
{
    auto shaderCode = filesystem::instance().read_file(filepath);

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderCode.size(),
        .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(deviceVK, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void vk_renderer::createFramebuffers()
{
    _frameBuffers.resize(_swapchain->imageViews.size());

    for (size_t i = 0; i < _swapchain->imageViews.size(); i++) {
        VkImageView attachments[] = {
            _swapchain->imageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = _swapchain->extent.width;
        framebufferInfo.height = _swapchain->extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(deviceVK, &framebufferInfo, nullptr, &_frameBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    std::printf("Created %zu frame buffers\n", _swapchain->imageViews.size());
}

void vk_renderer::prepareFrame(bool waitFence)
{
    if (waitFence)
    {
        vkWaitForFences(deviceVK, 1, &_waitFences[_currentBuffer], VK_TRUE, UINT64_MAX);
        vkResetFences(deviceVK, 1, &_waitFences[_currentBuffer]);
    }

    VkResult result = _swapchain->acquireNextImage(_presentCompleteSemaphores[_currentBuffer], _currentImageIndex);

    // TODO: check resize
}

void vk_renderer::buildCommandBuffer()
{
    VkCommandBuffer commandBuffer = _drawCmdBuffers[_currentBuffer];

    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _renderPass;
    renderPassInfo.framebuffer = _frameBuffers[_currentImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _swapchain->extent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_swapchain->extent.width);
    viewport.height = static_cast<float>(_swapchain->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _swapchain->extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void vk_renderer::submitFrame(bool skipQueueSubmit)
{
    if (!skipQueueSubmit)
    {
        const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &_presentCompleteSemaphores[_currentBuffer],
            .pWaitDstStageMask = &waitPipelineStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &_drawCmdBuffers[_currentBuffer],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &_renderCompleteSemaphores[_currentImageIndex]
        };
        if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _waitFences[_currentBuffer]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
    }

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &_renderCompleteSemaphores[_currentImageIndex],
        .swapchainCount = 1,
        .pSwapchains = &_swapchain->swapChain,
        .pImageIndices = &_currentImageIndex,
        .pResults = nullptr // Optional
    };

    // vkQueuePresentKHR(_presentQueue, &presentInfo);
    vkQueuePresentKHR(_graphicsQueue, &presentInfo);

    // TODO: resize

    // Select the next frame to render to, based on the max. no. of concurrent frames
	_currentBuffer = (_currentBuffer + 1) % maxConcurrentFrames;
}