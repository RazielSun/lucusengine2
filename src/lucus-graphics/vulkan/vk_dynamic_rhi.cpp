#include "vk_dynamic_rhi.hpp"

#include "engine_info.hpp"
#include "window_manager.hpp"

#include "command_buffer.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"
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
    // vkDestroyDescriptorSetLayout(_deviceHandle, _frameDescriptorSetLayout, nullptr);
    // vkDestroyDescriptorSetLayout(_deviceHandle, _objectDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(_deviceHandle, _texturesDescriptorSetLayout, nullptr);

    for (auto& ubDesc : _ubDescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(_deviceHandle, ubDesc.second, nullptr);
    }

    for (auto& buffer : _uniformBuffers) {
        buffer.cleanup();
    }
    _uniformBuffers.clear();

    for (auto& tex : _textures)
    {
        tex.second.cleanup();
    }
    _textures.clear();

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

    mainRenderPass.cleanup();

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

    auto& context = _contexts.emplace_back();
    context.init(_instance, _device->getGPU(), _deviceHandle, window);

    if (!mainRenderPass.bInitialized)
    {
        mainRenderPass.init(_deviceHandle, context.colorFormat, context.depthFormat);
    }

    context.init_framebuffers(mainRenderPass);

    window_context_handle out_handle(static_cast<u32>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

void vk_dynamic_rhi::getWindowContextSize(const window_context_handle& ctx_handle, u32& width, u32& height) const
{
    width = 0, height = 0;
    if (!ctx_handle.is_valid())
    {
        // TODO: error
        return;
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    width = ctx.swapChain.extent.width;
    height = ctx.swapChain.extent.height;
}

void vk_dynamic_rhi::execute(const window_context_handle& ctx_handle, u32 frameIndex, const gpu_command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    vk_window_context& ctx = _contexts[ctx_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;

    // here is wait fences & semaphores
    ctx.wait_frame(currentFrame);

    VkCommandBuffer& cmdBuffer = _commandbuffer_pool.getCommandBuffer(currentFrame);
    vkResetCommandBuffer(cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    for (u32 i = 0; i < cmd.commandCount; ++i)
    {
        const u8 *data = cmd.data.data() + i * COMMAND_FIXED_SIZE;
        const gpu_command_base* gpu_cmd = reinterpret_cast<const gpu_command_base*>(data);
        switch (gpu_cmd->type)
        {
            case gpu_command_type::RENDER_PASS_BEGIN:
                {
                    const auto* rb_cmd = reinterpret_cast<const gpu_render_pass_begin_command*>(data);

                    VkRenderPassBeginInfo renderPassInfo{};
                    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    renderPassInfo.renderPass = mainRenderPass.renderPass;
                    renderPassInfo.framebuffer = ctx.framebuffers.frameBuffers[ctx.currentImageIndex].framebuffer;
                    renderPassInfo.renderArea.offset = { rb_cmd->offset_x, rb_cmd->offset_y };
                    renderPassInfo.renderArea.extent = { rb_cmd->extent_x, rb_cmd->extent_y };

                    std::array<VkClearValue, 2> clearValues{};
                    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
                    clearValues[1].depthStencil = {1.0f, 0};

                    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                    renderPassInfo.pClearValues = clearValues.data();

                    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                }
                break;
            case gpu_command_type::RENDER_PASS_END:
                {
                    vkCmdEndRenderPass(cmdBuffer);
                }
                break;
            case gpu_command_type::SET_VIEWPORT:
                {
                    const auto* vp_cmd = reinterpret_cast<const gpu_set_viewport_command*>(data);

                    VkViewport viewport{};
                    viewport.x = float(vp_cmd->x);
                    viewport.y = float(vp_cmd->y + vp_cmd->height);
                    viewport.width = float(vp_cmd->width);
                    viewport.height = -float(vp_cmd->height); // Vulkan Flip Y
                    viewport.minDepth = 0.f;
                    viewport.maxDepth = 1.f;

                    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                }
                break;
            case gpu_command_type::SET_SCISSOR:
                {
                    const auto* sc_cmd = reinterpret_cast<const gpu_set_scissor_command*>(data);

                    VkRect2D scissor{};
                    scissor.offset = { sc_cmd->offset_x, sc_cmd->offset_y };
                    scissor.extent = { sc_cmd->extent_x, sc_cmd->extent_y };

                    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
                }
                break;
            case gpu_command_type::BIND_PIPELINE:
                {
                    const auto* bp_cmd = reinterpret_cast<const gpu_bind_pipeline_command*>(data);
                    auto found = _pipelineStates.find(bp_cmd->pso_handle.get());
                    vk_pipeline_state& pso = found->second;
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipeline());
                }
                break;
            case gpu_command_type::BIND_UNIFORM_BUFFER:
                {
                    const auto* bu_cmd = reinterpret_cast<const gpu_bind_uniform_buffer_command*>(data);
                    auto found = _pipelineStates.find(bu_cmd->pso_handle.get());
                    vk_pipeline_state& pso = found->second;
                    vk_uniform_buffer& ub = _uniformBuffers[bu_cmd->ub_handle.as_index()];
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)bu_cmd->position, 1, ub.get(currentFrame), 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_TEXTURE:
                {
                    const auto* bt_cmd = reinterpret_cast<const gpu_bind_texture_command*>(data);
                    auto found = _pipelineStates.find(bt_cmd->pso_handle.get());
                    vk_pipeline_state& pso = found->second;
                    auto found_tex = _textures.find(bt_cmd->tex_handle.get());
                    vk_texture& tex = found_tex->second; // TODO: descriptor set to texture
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)bt_cmd->position, 1, &tex.texDescriptorSet, 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_VERTEX_BUFFER:
                {
                    const auto* bv_cmd = reinterpret_cast<const gpu_bind_vertex_command*>(data);
                    auto found = _meshes.find(bv_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    VkBuffer vertexBuffers[] = { gpuMesh.vertexBuffer.get() };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
                }
                break;
            case gpu_command_type::DRAW_INDEXED:
                {
                    const auto* di_cmd = reinterpret_cast<const gpu_draw_indexed_command*>(data);
                    auto found = _meshes.find(di_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    vkCmdBindIndexBuffer(cmdBuffer, gpuMesh.indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmdBuffer, di_cmd->indexCount, 1, 0, 0, 0);
                }
                break;
            case gpu_command_type::DRAW_VERTEX:
                {
                    const auto* dv_cmd = reinterpret_cast<const gpu_draw_vertex_command*>(data);
                    vkCmdDraw(cmdBuffer, dv_cmd->vertexCount, 1, 0, 0);
                }
                break;
            default:
                {
                    std::printf("execute cmd doesn't handled %d\n", (u32)gpu_cmd->type);
                }
                break;
        }
    }

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

	const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ctx.frames[currentFrame].imageAvailable,
		.pWaitDstStageMask = &waitPipelineStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ctx.frames[currentFrame].renderFinished
	};
    
	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, ctx.frames[currentFrame].fence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx.frames[currentFrame].renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &ctx.swapChain.swapChain,
        .pImageIndices = &ctx.currentImageIndex,
        .pResults = nullptr // Optional
    };

    // vkQueuePresentKHR(_presentQueue, &presentInfo);
    vkQueuePresentKHR(_graphicsQueue, &presentInfo);

    vkDeviceWaitIdle(_deviceHandle);
}

void vk_dynamic_rhi::createCommandBufferPool()
{
    _commandbuffer_pool.init(_deviceHandle, _device->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT));
}

void vk_dynamic_rhi::createDescriptorPool()
{
    // 1 for frame UBO + maxObjectCount for object UBOs
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(g_framesInFlight) * (1 + g_maxObjectBufferCount) },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, g_maxTexturesCount },
        { VK_DESCRIPTOR_TYPE_SAMPLER,       g_maxSamplersCount },
    };

    uint32_t totalCount = 0;
    for (auto& size : poolSizes) {
        totalCount += size.descriptorCount;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = totalCount;
    
    if (vkCreateDescriptorPool(_deviceHandle, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::printf("Descriptor pool created successfully\n");
}

void vk_dynamic_rhi::createDescriptorSetLayouts()
{
    // {
    //     constexpr uint32_t bindingCount = 1;
    //     std::array<VkDescriptorSetLayoutBinding, bindingCount> bindings{};

    //     bindings[0] = VkDescriptorSetLayoutBinding
    //     {
    //         .binding = 0,
    //         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //         .descriptorCount = 1,
    //         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    //         .pImmutableSamplers = nullptr
    //     };

    //     VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //     layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    //     layoutInfo.pBindings = bindings.data();

    //     if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &_frameDescriptorSetLayout) != VK_SUCCESS) {
    //         throw std::runtime_error("failed to create descriptor set layout!");
    //     }

    //     std::printf("Frame Descriptor set layout created successfully\n");
    // }
    
    // {
    //     constexpr uint32_t bindingCount = 1;
    //     std::array<VkDescriptorSetLayoutBinding, bindingCount> bindings{};

    //     bindings[0] = VkDescriptorSetLayoutBinding
    //     {
    //         .binding = 0,
    //         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //         .descriptorCount = 1,
    //         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    //         .pImmutableSamplers = nullptr
    //     };

    //     VkDescriptorSetLayoutCreateInfo layoutInfo{};
    //     layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    //     layoutInfo.pBindings = bindings.data();

    //     if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &_objectDescriptorSetLayout) != VK_SUCCESS) {
    //         throw std::runtime_error("failed to create descriptor set layout!");
    //     }

    //     std::printf("Object Descriptor set layout created successfully\n");
    // }

    {
        constexpr uint32_t bindingCount = 2;
        std::array<VkDescriptorSetLayoutBinding, bindingCount> bindings{};

        bindings[0] = VkDescriptorSetLayoutBinding
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };
        bindings[1] = VkDescriptorSetLayoutBinding
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &_texturesDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        std::printf("Texture Descriptor set layout created successfully\n");
    }
}

pipeline_state_handle vk_dynamic_rhi::createPSO(material* mat)
{
    assert(mat);

    pipeline_state_handle pso_handle = pipeline_state_handle(mat->getHash());
    auto it = _pipelineStates.find(pso_handle.get());
    if (it != _pipelineStates.end()) {
        return pso_handle;
    }

    {
        auto pair = _pipelineStates.emplace(pso_handle.get(), _deviceHandle);
        assert(pair.second);

        vk_pipeline_state& pso = pair.first->second;

        vk_pipeline_state_desc init_desc;
        init_desc.renderPass = mainRenderPass.renderPass; // TODO: refactor render pass logic

        init_desc.layouts.reserve(3);
        if (mat->useFrameUniformBuffer())
        {
            init_desc.layouts.push_back(_ubDescriptorSetLayouts.find(uniform_buffer_type::FRAME)->second);
        }

        if (mat->useObjectUniformBuffer())
        {
            init_desc.layouts.push_back(_ubDescriptorSetLayouts.find(uniform_buffer_type::OBJECT)->second);
        }

        if (mat->getTexturesCount() > 0)
        {
            // TODO:
            init_desc.layouts.push_back(_texturesDescriptorSetLayout);
        }

        if (mat->useVertexIndexBuffers())
        {
            init_desc.bindingDesc = VkVertexInputBindingDescription {
                .binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, position),
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, texCoords),
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, color),
            };
        }

        const std::string& shaderName = mat->getShaderName();
        pso.init(shaderName, init_desc);

        std::printf("PSO %u created successfully\n", pso_handle.get());
    }

    return pso_handle;
}

mesh_handle vk_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh != nullptr);
    u32 meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(it->first);
    }

    auto pair = _meshes.emplace(meshHash, vk_mesh());
    assert(pair.second);

    vk_mesh& vkmsh = pair.first->second;
    vkmsh.init(_deviceHandle, _device->getGPU(), msh);

    std::printf("Mesh %u created successfully\n", meshHash);

    return mesh_handle(meshHash);
}

texture_handle vk_dynamic_rhi::loadTexture(texture* tex)
{
    assert(tex != nullptr);
    u32 texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    auto pair = _textures.emplace(texHash, vk_texture());
    assert(pair.second);

    vk_texture& vktex = pair.first->second;
    vktex.init(tex, _deviceHandle, _device->getGPU(), _texturesDescriptorSetLayout, _descriptorPool);

    transitionImageLayout(vktex.texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(vktex.stgBuffer, vktex.texImage, vktex.texExtent.width, vktex.texExtent.height);
    transitionImageLayout(vktex.texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vktex.free_staging();
    
    std::printf("Texture %u loaded successfully\n", texHash);

    return texture_handle(texHash);
}

uniform_buffer_handle vk_dynamic_rhi::createUniformBuffer(uniform_buffer_type ub_type, size_t bufferSize)
{
    VkDescriptorSetLayout descriptorSetLayout { VK_NULL_HANDLE };

    auto found = _ubDescriptorSetLayouts.find(ub_type);
    if (found != _ubDescriptorSetLayouts.end())
    {
        descriptorSetLayout = found->second;
    }
    else
    {
        auto result = _ubDescriptorSetLayouts.emplace(ub_type, VkDescriptorSetLayout());
        if (result.second)
        {
            auto& it = result.first;

            std::array<VkDescriptorSetLayoutBinding, 1> bindings{};
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
            layoutInfo.bindingCount = static_cast<u32>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(_deviceHandle, &layoutInfo, nullptr, &it->second) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor set layout!");
            }

            std::printf("UB Descriptor set layout created successfully\n");

            descriptorSetLayout = it->second;
        }
    }

    if (descriptorSetLayout == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to  description set for uniform buffer!");
    }

    vk_uniform_buffer& buffer = _uniformBuffers.emplace_back();
    
    buffer.init(_deviceHandle, _device->getGPU(), descriptorSetLayout, _descriptorPool, bufferSize);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));
    std::printf("Uniform buffer %d [Type: %d] created successfully\n", ub_handle.get(), (u32)ub_type);

    return ub_handle;
}

void vk_dynamic_rhi::getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr)
{
    // std::printf("getUniformBufferMemory %d [Ctx: %d]\n", ub_handle.get(), ctx_handle.get());
    if (!ub_handle.is_valid())
    {
        throw std::runtime_error("failed to get uniform buffer memory! ub is null!");
    }

    auto& buffer = _uniformBuffers[ub_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;
    memory_ptr = buffer.getMappedData(currentFrame);
}

void vk_dynamic_rhi::wait_idle()
{
    vkDeviceWaitIdle(_deviceHandle);
}

VkCommandBuffer vk_dynamic_rhi::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandbuffer_pool.get();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_deviceHandle, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void vk_dynamic_rhi::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphicsQueue);

    vkFreeCommandBuffers(_deviceHandle, _commandbuffer_pool.get(), 1, &commandBuffer);
}

void vk_dynamic_rhi::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void vk_dynamic_rhi::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // TODO
    barrier.dstAccessMask = 0; // TODO

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void vk_dynamic_rhi::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}