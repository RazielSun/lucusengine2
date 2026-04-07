#include "vk_dynamic_rhi.hpp"

#include "engine_info.hpp"
#include "window_manager.hpp"

#include "material.hpp"
#include "mesh.hpp"
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

    for (auto& buffer : _objectUniformBuffers) {
        buffer.cleanup();
    }
    _objectUniformBuffers.clear();

    for (auto& mesh : _meshes) {
        mesh.second.cleanup();
    }
    _meshes.clear();

    _pipelineStates.clear();

    _commandbuffer_pool.cleanup();
    
    for (auto& ctx : _contexts) {
        ctx.cleanup();
    }
    _contexts.clear();
    _contextHandles.clear();

    _device.reset();

    vkDestroyInstance(_instance, nullptr);
}

void vk_dynamic_rhi::init() 
{
    createInstance();
    createDevice();
    createCommandBufferPool();
    
    createDescriptorPool();
    createDescriptorSetLayouts();
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

window_context_handle vk_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to vk_swapchain::init");
	}

    vk_window_context context;
    context.init(_instance, _device->getGPU(), _deviceHandle, window);
    context.uniformbuffers.init(_deviceHandle, _device->getGPU(), _frameDescriptorSetLayout, _descriptorPool, sizeof(frame_uniform_buffer));

    _contexts.push_back(context);

    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

const std::vector<window_context_handle>& vk_dynamic_rhi::getWindowContexts() const
{
    return _contextHandles;
}

float vk_dynamic_rhi::getWindowContextAspectRatio(const window_context_handle& handle) const
{
    if (!handle.is_valid()) {
        return 1.0f;
    }

    const auto& ctx = _contexts[handle.as_index()];
    return static_cast<float>(ctx.swapChain.extent.width) / static_cast<float>(ctx.swapChain.extent.height);
}

void vk_dynamic_rhi::beginFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];
    ctx.wait_frame();
}

void vk_dynamic_rhi::submit(const window_context_handle& ctx_handle, const command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    vk_window_context& ctx = _contexts[ctx_handle.as_index()];

    VkCommandBuffer& cmdBuffer = _commandbuffer_pool.getCommandBuffer(ctx.currentFrame);

    vkResetCommandBuffer(cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ctx.renderPass.renderPass;
    renderPassInfo.framebuffer = ctx.framebuffers.frameBuffers[ctx.currentImageIndex].framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = ctx.swapChain.extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmdBuffer, 0, 1, &ctx.viewport.viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &ctx.viewport.scissor);

    // HACK: clip space Y for Vulkan
    auto frame_ubo = cmd.frame_ubo;
    frame_ubo.proj[1][1] *= -1;
    ctx.uniformbuffers.write(ctx.currentFrame, &frame_ubo, sizeof(frame_ubo));

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;
        vk_uniform_buffer& buffer = _objectUniformBuffers[object_id.as_index()];

        buffer.write(ctx.currentFrame, &renderInstance.object_data, sizeof(renderInstance.object_data));

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            auto psoIt = _pipelineStates.find(material_id.get());
            if (psoIt != _pipelineStates.end()) {
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipeline());
                if (psoIt->second.isUniformBufferUsed())
                {
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipelineLayout(), (uint32_t)shader_binding::frame, 1, ctx.uniformbuffers.get(ctx.currentFrame), 0, nullptr);
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psoIt->second.getPipelineLayout(), (uint32_t)shader_binding::object, 1, buffer.get(ctx.currentFrame), 0, nullptr);
                }
            } else {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }

        const auto& mesh_id = renderInstance.mesh;
        if (mesh_id.is_valid())
        {
            auto meshIt = _meshes.find(mesh_id.get());
            if (meshIt != _meshes.end())
            {
                auto& gpuMesh = meshIt->second;
                if (gpuMesh.bHasVertexData)
                {
                    VkBuffer vertexBuffers[] = { gpuMesh.vertexBuffer.get() };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
                }

                if (gpuMesh.indexCount > 0)
                {
                    vkCmdBindIndexBuffer(cmdBuffer, gpuMesh.indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmdBuffer, gpuMesh.indexCount, 1, 0, 0, 0);
                }
                else if (gpuMesh.vertexCount > 0)
                {
                    vkCmdDraw(cmdBuffer, gpuMesh.vertexCount, 1, 0, 0);
                }
            }
        }
    }

    vkCmdEndRenderPass(cmdBuffer);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

	const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ctx.frames[ctx.currentFrame].imageAvailable,
		.pWaitDstStageMask = &waitPipelineStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ctx.frames[ctx.currentFrame].renderFinished
	};
    
	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, ctx.frames[ctx.currentFrame].fence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx.frames[ctx.currentFrame].renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &ctx.swapChain.swapChain,
        .pImageIndices = &ctx.currentImageIndex,
        .pResults = nullptr // Optional
    };

    // vkQueuePresentKHR(_presentQueue, &presentInfo);
    vkQueuePresentKHR(_graphicsQueue, &presentInfo);
}

void vk_dynamic_rhi::endFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    vk_window_context& ctx = _contexts[ctx_handle.as_index()];

    ctx.currentFrame = (ctx.currentFrame + 1) % g_framesInFlight;

    wait_idle();
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

material_handle vk_dynamic_rhi::createMaterial(material* mat)
{
    assert(mat != nullptr);
    uint64_t shaderHash = mat->getHash();

    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        return material_handle(it->first);
    }

    _pipelineStates.emplace(shaderHash, _deviceHandle);

    it = _pipelineStates.find(shaderHash);

    auto& renderPass = _contexts[0].renderPass; // HACK: using first context's render pass for now
    
    if (mat->isUseUniformBuffers())
    {
        std::array<VkDescriptorSetLayout, 2> layouts = {
            _frameDescriptorSetLayout,
            _objectDescriptorSetLayout
        };
        it->second.init(mat, renderPass.renderPass, static_cast<uint32_t>(layouts.size()), layouts.data());
    }
    else
    {
        it->second.init(mat, renderPass.renderPass);
    }

    std::printf("Material %llu created successfully\n", static_cast<unsigned long long>(shaderHash));

    return material_handle(shaderHash);
}

mesh_handle vk_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh != nullptr);
    uint64_t meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(it->first);
    }

    _meshes.emplace(meshHash, vk_mesh());

    it = _meshes.find(meshHash);

    it->second.init(_deviceHandle, _device->getGPU(), msh);

    std::printf("Mesh %llu created successfully\n", static_cast<unsigned long long>(meshHash));

    return mesh_handle(meshHash);
}

render_object_handle vk_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    _objectUniformBuffers.emplace_back();
    vk_uniform_buffer& buffer = _objectUniformBuffers.back();
    buffer.init(_deviceHandle, _device->getGPU(), _objectDescriptorSetLayout, _descriptorPool, sizeof(object_uniform_buffer));

    std::printf("Uniform buffer for object %p created successfully\n", obj);

    return render_object_handle(static_cast<uint32_t>(_objectUniformBuffers.size()));
}

void vk_dynamic_rhi::wait_idle()
{
    vkDeviceWaitIdle(_deviceHandle);
}