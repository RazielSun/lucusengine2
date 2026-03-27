#include "vk_dynamic_rhi.hpp"

#include "engine_info.hpp"
#include "window_manager.hpp"

#include "material.hpp"
#include "vk_pipeline_state.hpp"

#include "glfw_include.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<vk_dynamic_rhi>();
    }
}

using namespace lucus;

vk_dynamic_rhi::vk_dynamic_rhi()
{
}

vk_dynamic_rhi::~vk_dynamic_rhi()
{
    vkDestroyDescriptorPool(_deviceHandle, _descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(_deviceHandle, _frameDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(_deviceHandle, _objectDescriptorSetLayout, nullptr);

    _frameUniformBuffer.cleanup();
    for (auto& buffer : _objectUniformBuffers) {
        buffer.cleanup();
    }
    _objectUniformBuffers.clear();

    for (auto& fence : _waitFences) {
        vkDestroyFence(_deviceHandle, fence, nullptr);
    }
    
    for (auto& semaphore : _renderCompleteSemaphores) {
        vkDestroySemaphore(_deviceHandle, semaphore, nullptr);
    }

    for (auto& semaphore : _presentCompleteSemaphores) {
        vkDestroySemaphore(_deviceHandle, semaphore, nullptr);
    }

	for (auto& framebuffer : _frameBuffers) {
        framebuffer.cleanup(_deviceHandle);
    }
    _frameBuffers.clear();

    for (auto& renderPass : _renderPasses) {
        renderPass.cleanup(_deviceHandle);
    }
    _renderPasses.clear();

    _pipelineStates.clear();

    _commandbuffer_pool.cleanup(_deviceHandle);
    
    for (auto& viewport : _viewports) {
        viewport.cleanup(_instance, _deviceHandle);
    }
    _viewports.clear();

    _device.reset();

    vkDestroyInstance(_instance, nullptr);
}

void vk_dynamic_rhi::init() 
{
    createInstance();
    createDevice();
    createCommandBufferPool();
    createSyncObjectsStable();
    
    createDescriptorPool();
    createDescriptorSetLayouts();
    createFrameUniformBuffers();
}

void vk_dynamic_rhi::createInstance()
{
    engine_info& engine_info = engine_info::instance();

    VkApplicationInfo appInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = engine_info.name().c_str(),// app_info.app_name().c_str(),
		.pEngineName = engine_info.name().c_str(),
		.apiVersion = VK_API_VERSION_1_1
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

    #ifndef NDEBUG
    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
	// Note that on Android this layer requires at least NDK r20
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	{
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
    #endif

    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    std::printf("VkInstance created successfully\n");
}

void vk_dynamic_rhi::createDevice()
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

    VkPhysicalDevice physicalDevice = physicalDevices[selectedDevice];

    // getEnabledFeatures();
    // getEnabledExtensions();

    // Vulkan device creation
    _device = std::make_unique<vk_device>(physicalDevice);

    //
    _device->createLogicalDevice(_enabledFeatures, _enabledDeviceExtensions);
    _deviceHandle = _device->getDevice();

    // Get a graphics queue from the device
    vkGetDeviceQueue(_deviceHandle, _device->queueFamilyIndices.graphics, 0, &_graphicsQueue);
    // vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);
}

viewport_handle vk_dynamic_rhi::createViewport(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to vk_swapchain::init");
	}

    vk_viewport viewport;
    viewport.init(_instance, _device->getGPU(), _deviceHandle, window);

    _viewports.push_back(viewport);

    viewport_handle out_handle(static_cast<uint32_t>(_viewports.size()));

    createSyncObjectsBy(viewport.imageCount);

    // TODO: ?
    vk_render_pass renderPass;
    vk_framebuffer framebuffer;
    getOrCreateRenderPassAndFramebuffer(viewport, renderPass, framebuffer);

    return out_handle;
}

void vk_dynamic_rhi::beginFrame(const viewport_handle& handle)
{
    if (!handle.is_valid()) {
        return;
    }

    vkWaitForFences(_deviceHandle, 1, &_waitFences[_currentBufferIndex], VK_TRUE, UINT64_MAX);
    vkResetFences(_deviceHandle, 1, &_waitFences[_currentBufferIndex]);
    
    _currentViewport = handle;
    vk_viewport& viewport = _viewports[_currentViewport.as_index()];

    vkAcquireNextImageKHR(_deviceHandle, viewport.swapChain, UINT64_MAX, _presentCompleteSemaphores[_currentBufferIndex], (VkFence)nullptr, &_currentImageIndex);
}

void vk_dynamic_rhi::endFrame()
{
    _currentBufferIndex = (_currentBufferIndex + 1) % g_framesInFlight;

    wait_idle();
}

void vk_dynamic_rhi::submit(const command_buffer& cmd)
{
    if (!_currentViewport.is_valid()) {
        return;
    }

    vk_viewport& viewport = _viewports[_currentViewport.as_index()];

    VkCommandBuffer& cmdBuffer = _commandbuffer_pool.getCommandBuffer(_currentBufferIndex);

    vkResetCommandBuffer(cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    vk_render_pass renderPass;
    vk_framebuffer framebuffer;
    getOrCreateRenderPassAndFramebuffer(viewport, renderPass, framebuffer);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass.renderPass;
    renderPassInfo.framebuffer = framebuffer.frameBuffers[_currentImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = viewport.extent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport.viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &viewport.scissor);

    // HACK: clip space Y for Vulkan
    auto frame_ubo = cmd.frame_ubo;
    frame_ubo.proj[1][1] *= -1;
    _frameUniformBuffer.write(_currentBufferIndex, &frame_ubo, sizeof(frame_ubo));

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;
        vk_buffer& buffer = _objectUniformBuffers[object_id.as_index()];

        buffer.write(_currentBufferIndex, &renderInstance.object_data, sizeof(renderInstance.object_data));

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end()) {
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipeline());
                if (psoIt->second.isUniformBufferUsed())
                {
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipelineLayout(), 0, 1, _frameUniformBuffer.getDescriptorSet(_currentBufferIndex), 0, nullptr);
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipelineLayout(), 1, 1, buffer.getDescriptorSet(_currentBufferIndex), 0, nullptr);
                }
            } else {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }

        const auto& mesh = renderInstance.mesh;
        // TODO: Bind vertex/index buffers and draw
        // TODO: mesh_handle = mesh.getDrawCount() is for test only
        vkCmdDraw(cmdBuffer, mesh.get(), 1, 0, 0);
    }

    vkCmdEndRenderPass(cmdBuffer);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    uint32_t imageIndex = _currentImageIndex;

	const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_presentCompleteSemaphores[_currentBufferIndex],
		.pWaitDstStageMask = &waitPipelineStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &_renderCompleteSemaphores[_currentBufferIndex]
	};
    
	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _waitFences[_currentBufferIndex]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &_renderCompleteSemaphores[_currentBufferIndex],
        .swapchainCount = 1,
        .pSwapchains = &viewport.swapChain,
        .pImageIndices = &_currentImageIndex,
        .pResults = nullptr // Optional
    };

    // vkQueuePresentKHR(_presentQueue, &presentInfo);
    vkQueuePresentKHR(_graphicsQueue, &presentInfo);
}

void vk_dynamic_rhi::createCommandBufferPool()
{
    _commandbuffer_pool.init(_deviceHandle, _device->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT));
}

void vk_dynamic_rhi::createDescriptorPool()
{
    uint32_t totalCount = static_cast<uint32_t>(g_framesInFlight) * (1 + g_maxObjectBufferCount); // 1 for frame UBO + maxObjectCount for object UBOs
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = totalCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = totalCount;
    
    if (vkCreateDescriptorPool(_deviceHandle, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::printf("Descriptor pool created successfully\n");
}

void vk_dynamic_rhi::createDescriptorSetLayouts()
{
    {
        constexpr uint32_t bindingCount = 1;
        std::array<VkDescriptorSetLayoutBinding, bindingCount> bindings{};

        bindings[0] = VkDescriptorSetLayoutBinding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &_frameDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        std::printf("Frame Descriptor set layout created successfully\n");
    }
    
    {
        constexpr uint32_t bindingCount = 1;
        std::array<VkDescriptorSetLayoutBinding, bindingCount> bindings{};

        bindings[0] = VkDescriptorSetLayoutBinding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &_objectDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        std::printf("Object Descriptor set layout created successfully\n");
    }
}

void vk_dynamic_rhi::createFrameUniformBuffers()
{
    _frameUniformBuffer.init(_deviceHandle, _device->getGPU(), _frameDescriptorSetLayout, _descriptorPool, sizeof(frame_uniform_buffer));

    std::printf("Frame uniform buffer created successfully\n");
}

void vk_dynamic_rhi::createFramebuffer(const vk_viewport& viewport, const vk_render_pass& renderPass)
{
    vk_framebuffer framebuffer;
    framebuffer.init(_deviceHandle, viewport, renderPass.renderPass);
    _frameBuffers.push_back(framebuffer);
}

void vk_dynamic_rhi::createSyncObjectsStable()
{
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    // Wait fences to sync command buffer access
    for (auto& fence : _waitFences) {
        if (vkCreateFence(_deviceHandle, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    // Used to ensure that image presentation is complete before starting to submit again
    for (auto& semaphore : _presentCompleteSemaphores) {
        if (vkCreateSemaphore(_deviceHandle, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    std::printf("Semaphores and fences [Stable] created successfully\n");
}

void vk_dynamic_rhi::createSyncObjectsBy(int imageCount)
{
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    // Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	_renderCompleteSemaphores.resize(imageCount);
    for (auto& semaphore : _renderCompleteSemaphores) {
        if (vkCreateSemaphore(_deviceHandle, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    std::printf("Semaphores [ImageCount: %d] created successfully\n", imageCount);
}

material_handle vk_dynamic_rhi::createMaterial(material* mat)
{
    assert(mat != nullptr);
    uint32_t shaderHash = mat->getHash();

    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        return material_handle(it->first);
    }

    _pipelineStates.emplace(shaderHash, _deviceHandle);

    it = _pipelineStates.find(shaderHash);

    int renderPassIndex = mat->getRenderPass();
    if (renderPassIndex < 0 || renderPassIndex >= static_cast<int>(_renderPasses.size())) {
        std::cerr << "Invalid render pass index in material: " << renderPassIndex << std::endl;
        return material_handle();
    }
    
    if (mat->isUseUniformBuffers())
    {
        std::array<VkDescriptorSetLayout, 2> layouts = {
            _frameDescriptorSetLayout,
            _objectDescriptorSetLayout
        };
        it->second.init(mat, _renderPasses[renderPassIndex].renderPass, static_cast<uint32_t>(layouts.size()), layouts.data());
    }
    else
    {
        it->second.init(mat, _renderPasses[renderPassIndex].renderPass);
    }

    std::printf("Material %d created successfully\n", shaderHash);

    return material_handle(shaderHash);
}

render_object_handle vk_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    _objectUniformBuffers.emplace_back();
    vk_buffer& buffer = _objectUniformBuffers.back();
    buffer.init(_deviceHandle, _device->getGPU(), _objectDescriptorSetLayout, _descriptorPool, sizeof(object_uniform_buffer));

    std::printf("Uniform buffer for object %p created successfully\n", obj);

    return render_object_handle(static_cast<uint32_t>(_objectUniformBuffers.size()));
}

void vk_dynamic_rhi::getOrCreateRenderPassAndFramebuffer(const vk_viewport& viewport, vk_render_pass& outRenderPass, vk_framebuffer& outFramebuffer)
{
    int index = -1;
    for (size_t i = 0; i < _renderPasses.size(); i++) {
        if (_renderPasses[i].colorFormat == viewport.colorFormat) {
            index = static_cast<int>(i);
            break;
        }
    }

    if (index == -1) {
        index = static_cast<int>(_renderPasses.size());

        _renderPasses.emplace_back();
        vk_render_pass& newRenderPass = _renderPasses.back();
        newRenderPass.init(_deviceHandle, viewport.colorFormat);

        createFramebuffer(viewport, newRenderPass);
    }

    outRenderPass = _renderPasses[index];
    outFramebuffer = _frameBuffers[index];
}

void vk_dynamic_rhi::wait_idle()
{
    vkDeviceWaitIdle(_deviceHandle);
}