#include "vk_dynamic_rhi.hpp"

#include "engine_info.hpp"
#include "window_manager.hpp"

#include "command_buffer.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include "vk_pipeline_state.hpp"
#include "vk_utils.hpp"

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
    vkDeviceWaitIdle(_deviceHandle);

    vkDestroyDescriptorPool(_deviceHandle, _descriptorPool, nullptr);
    for (auto& desc : _descriptors)
    {
        desc.cleanup();
    }
    _descriptors.clear();

    for (auto& buffer : _uniformBuffers) {
        buffer.cleanup();
    }
    _uniformBuffers.clear();

    for (auto& tex : _textures)
    {
        tex.second.cleanup();
    }
    _textures.clear();

    for (auto& smpl: _samplers)
    {
        smpl.cleanup();
    }
    _samplers.clear();

    for (auto& mesh : _meshes) {
        mesh.second.cleanup();
    }
    _meshes.clear();

    _pipelineStates.clear();

    for (auto& rt : _renderTargets)
    {
        rt.cleanup();
    }
    _renderTargets.clear();

    _commandbuffer_pool.cleanup();

    for (auto& frame_sync : frameSync)
    {
        frame_sync.cleanup();
    }
    
    for (auto& ctx : _contexts) {
        ctx.cleanup();
    }
    _contexts.clear();
    _contextHandles.clear();

    for (auto& pass : _renderPasses)
    {
        pass.cleanup();
    }
    _renderPasses.clear();

    _device.reset();

    vkDestroyInstance(_instance, nullptr);
}

void vk_dynamic_rhi::init() 
{
    createInstance();
    createDevice();
    createCommandBufferPool();

    for (auto& frame_sync : frameSync)
    {
        frame_sync.init(_deviceHandle);
    }
    
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

    depthFormat = utils::findDepthFormat(physicalDevice);
}

void vk_dynamic_rhi::createPasses()
{
    vk_render_pass_desc main_desc{
        .name = render_pass_name::FORWARD_PASS,
        .colorAttachmentCount = 1,
        .colorFormats[0] = colorFormat,
        .depthAttachmentCount = 1,
        .depthFormat = depthFormat
    };
    _renderPasses.emplace_back().init(_deviceHandle, main_desc);

    vk_render_pass_desc shadow_desc{
        .name = render_pass_name::SHADOW_PASS,
        .colorAttachmentCount = 0,
        .depthAttachmentCount = 1,
        .depthFormat = depthFormat
    };
    _renderPasses.emplace_back().init(_deviceHandle, shadow_desc);

    bPassesInitialized = true;
}

window_context_handle vk_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to vk_swapchain::init");
	}

    auto& context = _contexts.emplace_back();
    context.init(_instance, _device->getGPU(), _deviceHandle, window);

    if (!bPassesInitialized)
    {
        colorFormat = context.selectedFormat.format;
        createPasses();
    }

    auto& rt = _renderTargets.emplace_back();
    render_target_handle rt_handle(static_cast<u32>(_renderTargets.size()));

    window_context_handle out_handle(static_cast<u32>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    render_target_info info{
        .frame_count = context.swapChain.imageCount,
        .width = context.swapChain.extent.width,
        .height = context.swapChain.extent.height,
        .target_count = 2,
        .targets = { render_target_type::COLOR, render_target_type::DEPTH },
        .passName = render_pass_name::FORWARD_PASS
    };

    context.rt_handle = createRenderTarget(info, out_handle);

    return out_handle;
}

void vk_dynamic_rhi::getWindowContextSize(const window_context_handle& ctx_handle, u32& width, u32& height) const
{
    width = 0, height = 0;
    if (!ctx_handle.is_valid())
    {
        throw std::runtime_error("getWindowContextSize:: we have invalid window context!");
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    width = ctx.swapChain.extent.width;
    height = ctx.swapChain.extent.height;
}

render_target_handle vk_dynamic_rhi::getWindowContextRenderTarget(const window_context_handle& ctx_handle) const
{
    if (!ctx_handle.is_valid())
    {
        throw std::runtime_error("getWindowContextRenderTarget:: we have invalid window context!");
    }
    const auto& ctx = _contexts[ctx_handle.as_index()];
    return ctx.rt_handle;
}

void vk_dynamic_rhi::execute(const window_context_handle& ctx_handle, u32 frameIndex, const gpu_command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    vk_window_context& ctx = _contexts[ctx_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;

    vkWaitForFences(_deviceHandle, 1, &frameSync[currentFrame].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(_deviceHandle, 1, &frameSync[currentFrame].fence);

    ctx.acquire_image(frameIndex);

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
                    auto found = std::find_if(_renderPasses.begin(), _renderPasses.end(), [passName = rb_cmd->pass](const vk_render_pass& pass) {
                        return passName == pass.name;
                    });
                    assert(found != _renderPasses.end());
                    renderPassInfo.renderPass = found->renderPass;

                    renderPassInfo.renderArea.offset = { 0, 0 };
                    renderPassInfo.renderArea.extent = { rb_cmd->width, rb_cmd->height };
                    
                    std::vector<VkClearValue> clearValues;
                    std::set<render_target_handle> uniqueRenderTargets;
                    if (rb_cmd->colorTargetCount > 0)
                    {
                        assert(rb_cmd->colorTargetCount <= g_maxMrtColorTargets);

                        // TODO: support multiple render targets
                        clearValues.emplace_back().color = {{0.0f, 0.0f, 0.0f, 1.0f}};

                        auto rt_handle = rb_cmd->colorTargets[0];
                        assert(rt_handle.is_valid());

                        // uniqueRenderTargets.insert(rt_handle);

                        auto& rt = _renderTargets[rt_handle.as_index()];
                        assert(rt.bFramebuffer);

                        const u32 index = rt.bSwapChain ? ctx.currentImageIndex : currentFrame;

                        assert(rt.frameBuffers.size() > index);
                        renderPassInfo.framebuffer = rt.frameBuffers[index];
                    }

                    if (rb_cmd->depthTargetCount > 0)
                    {
                        assert(rb_cmd->depthTargetCount == 1);

                        clearValues.emplace_back().depthStencil = {1.0f, 0};

                        auto rt_handle = rb_cmd->depthTarget;
                        assert(rt_handle.is_valid());

                        // if (uniqueRenderTargets.find(rt_handle) == uniqueRenderTargets.end())
                        {
                            // uniqueRenderTargets.insert(rt_handle);
                            auto& rt = _renderTargets[rt_handle.as_index()];
                            assert(rt.bFramebuffer);

                            const u32 index = rt.bSwapChain ? ctx.currentImageIndex : currentFrame;

                            assert(rt.frameBuffers.size() > index);
                            renderPassInfo.framebuffer = rt.frameBuffers[index];
                        }
                    }

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
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipeline());
                }
                break;
            case gpu_command_type::BIND_UNIFORM_BUFFER:
                {
                    const auto* bu_cmd = reinterpret_cast<const gpu_bind_uniform_buffer_command*>(data);
                    auto found = _pipelineStates.find(bu_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    auto& ub = _uniformBuffers[bu_cmd->ub_handle.as_index()];
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)bu_cmd->position, 1, ub.get(currentFrame), 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_TEXTURE:
                {
                    const auto* bt_cmd = reinterpret_cast<const gpu_bind_texture_command*>(data);
                    auto found = _pipelineStates.find(bt_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    auto found_tex = _textures.find(bt_cmd->tex_handle.get());
                    auto& tex = found_tex->second;
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)bt_cmd->position, 1, &tex.descriptorSet, 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_RENDER_TARGET:
                {
                    const auto* br_cmd = reinterpret_cast<const gpu_bind_render_target_command*>(data);
                    auto found = _pipelineStates.find(br_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    auto rt_handle = br_cmd->rt_handle;
                    assert(rt_handle.is_valid());
                    auto& rt = _renderTargets[rt_handle.as_index()];
                    const u32 index = rt.bSwapChain ? ctx.currentImageIndex : currentFrame;
                    
                    assert(rt.attachments.size() > br_cmd->slot);
                    auto& attachment = rt.attachments[br_cmd->slot];
                    VkDescriptorSet descriptorSet = attachment.descriptorSets[index];
                    // check for COLOR or DEPTH?
                    // if (br_cmd->type == render_target_type::COLOR)
                    // else if (br_cmd->type == render_target_type::DEPTH)

                    assert(descriptorSet);
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)br_cmd->position, 1, &descriptorSet, 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_SAMPLER:
                {
                    const auto* bs_cmd = reinterpret_cast<const gpu_bind_sampler_command*>(data);
                    auto found = _pipelineStates.find(bs_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    auto& smpl = _samplers[bs_cmd->smpl_handle.as_index()];
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso.getPipelineLayout(), (u32)bs_cmd->position, 1, &smpl.descriptorSet, 0, nullptr);
                }
                break;
            case gpu_command_type::BIND_DESCRIPTION_TABLE:
                {
                    // Nothing to do
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
            case gpu_command_type::IMAGE_BARRIER:
                {
                    const auto* ti_cmd = reinterpret_cast<const gpu_image_barrier_command*>(data);
                    auto rt_handle = ti_cmd->rt_handle;
                    assert(rt_handle.is_valid());
                    auto& rt = _renderTargets[rt_handle.as_index()];

                    assert(rt.attachments.size() > ti_cmd->slot);
                    auto& attachment = rt.attachments[ti_cmd->slot];

                    const u32 index = rt.bSwapChain ? ctx.currentImageIndex : currentFrame;
                    VkImage image = attachment.images[index];
                    VkImageAspectFlags aspect = attachment.aspectFlags;

                    assert(attachment.states.size() > index);

                    const resource_state trackedState = attachment.states[index];
                    const auto src = utils::toVkBarrierState(trackedState);
                    const auto dst = utils::toVkBarrierState(ti_cmd->dst);

                    VkImageMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.oldLayout = src.layout;
                    barrier.newLayout = dst.layout;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = image;
                    barrier.subresourceRange.aspectMask = aspect;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.srcAccessMask = src.access;
                    barrier.dstAccessMask = dst.access;

                    vkCmdPipelineBarrier(
                        cmdBuffer,
                        src.stage,
                        dst.stage,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);

                    attachment.states[index] = ti_cmd->dst;
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

    u32 imageIndex = frameIndex % g_swapchainImageCount;
	const VkPipelineStageFlags waitPipelineStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ctx.imageSync[imageIndex].imageAvailable,
		.pWaitDstStageMask = &waitPipelineStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmdBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ctx.imageSync[imageIndex].renderFinished
	};
    
	if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, frameSync[currentFrame].fence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx.imageSync[imageIndex].renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &ctx.swapChain.swapChain,
        .pImageIndices = &ctx.currentImageIndex,
        .pResults = nullptr // Optional
    };

    // vkQueuePresentKHR(_presentQueue, &presentInfo);
    vkQueuePresentKHR(_graphicsQueue, &presentInfo);

    // vkDeviceWaitIdle(_deviceHandle);
}

void vk_dynamic_rhi::createCommandBufferPool()
{
    _commandbuffer_pool.init(_deviceHandle, _device->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT));
}

void vk_dynamic_rhi::createDescriptorPool()
{
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(g_framesInFlight) * g_maxUniformBufferCount },
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
    _descriptors.emplace_back().init(_deviceHandle, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shader_binding_stage::FRAGMENT);
    _descriptors.emplace_back().init(_deviceHandle, VK_DESCRIPTOR_TYPE_SAMPLER, shader_binding_stage::FRAGMENT);
    _descriptors.emplace_back().init(_deviceHandle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_binding_stage::VERTEX);
    _descriptors.emplace_back().init(_deviceHandle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_binding_stage::FRAGMENT);
    _descriptors.emplace_back().init(_deviceHandle, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_binding_stage::BOTH);
}

pipeline_state_handle vk_dynamic_rhi::createPSO(material* mat, render_pass_name passName)
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

        auto found = std::find_if(_renderPasses.begin(), _renderPasses.end(), [passName](const vk_render_pass& pass) {
            return passName == pass.name;
        });
        assert(found != _renderPasses.end());
        init_desc.renderPass = found->renderPass;
        init_desc.bUseColor = passName != render_pass_name::SHADOW_PASS;

        auto bufferDSLVertex = findDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_binding_stage::VERTEX);
        assert(bufferDSLVertex != VK_NULL_HANDLE);

        auto bufferDSLBoth = findDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_binding_stage::BOTH);
        assert(bufferDSLBoth != VK_NULL_HANDLE);

        auto textureDSL = findDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shader_binding_stage::FRAGMENT);
        assert(textureDSL != VK_NULL_HANDLE);

        auto samplerDSL = findDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER, shader_binding_stage::FRAGMENT);
        assert(samplerDSL != VK_NULL_HANDLE);

        // this is strict layouts shader contract
        init_desc.layouts = {
            bufferDSLVertex, // VIEW
            bufferDSLVertex, // OBJECT
            bufferDSLBoth, // LIGHT
            textureDSL, // TEXTURE
            textureDSL, // SHADOW_MAP
            samplerDSL, // SAMPLER
            samplerDSL // SHADOW_MAP SAMPLER
        };

        if (mat->useVertexIndexBuffers())
        {
            init_desc.bindingDesc = VkVertexInputBindingDescription {
                .binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = (u32)init_desc.attributes.size(),
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, position),
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = (u32)init_desc.attributes.size(),
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex, normal),
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = (u32)init_desc.attributes.size(),
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, texCoords),
            };

            init_desc.attributes.emplace_back() = VkVertexInputAttributeDescription{
                .location = (u32)init_desc.attributes.size(),
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

render_target_handle vk_dynamic_rhi::createRenderTarget(const const render_target_info& info, const window_context_handle& ctx_handle)
{
    auto& rt = _renderTargets.emplace_back();
    render_target_handle rt_handle(static_cast<u32>(_renderTargets.size()));

    bool bNeedDescritors = true;
    if (ctx_handle.is_valid())
    {
        auto& ctx = _contexts[ctx_handle.as_index()];
        ctx.init_images(rt);
        rt.bSwapChain = true;
        bNeedDescritors = false;
    }

    auto found = std::find_if(_renderPasses.begin(), _renderPasses.end(), [passName = info.passName](const vk_render_pass& pass) {
        return passName == pass.name;
    });
    assert(found != _renderPasses.end());

    // bool bUseColor = type == render_target_type::COLOR_ONLY || type == render_target_type::COLOR_AND_DEPTH;
    // bool bUseDepth = type == render_target_type::DEPTH_ONLY || type == render_target_type::COLOR_AND_DEPTH;
    auto textureDSL = findDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shader_binding_stage::FRAGMENT);
	vk_render_target_desc init_desc {
		.count = info.frame_count,
		.extent = VkExtent2D{info.width, info.height},
		.bUseFramebuffer = true,
		.renderPass = found->renderPass,
        .descriptorPool = _descriptorPool,
        .descriptorSetLayout = textureDSL
	};

    for (u32 i = 0; i < info.target_count; i++)
    {
        vk_images_desc_info img_desc{
            .bIsColor = info.targets[i] == render_target_type::COLOR,
            .bNeedDescriptorSet = bNeedDescritors,
            .format = info.targets[i] == render_target_type::COLOR ? colorFormat : depthFormat,
        };
        init_desc.infos.push_back(img_desc);
    }

	rt.init(_deviceHandle, _device->getGPU(), init_desc);

    std::printf("Render Target %u created successfully\n", rt_handle.get());

    return rt_handle;
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

sampler_handle vk_dynamic_rhi::createSampler()
{
    auto samplerDSL = findDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER, shader_binding_stage::FRAGMENT);
    assert(samplerDSL != VK_NULL_HANDLE);

    vk_sampler& vksmpl = _samplers.emplace_back();
    vksmpl.init(_deviceHandle, samplerDSL, _descriptorPool);

    sampler_handle smpl_handle(static_cast<u32>(_samplers.size()));
    std::printf("Sampler %d created successfully\n", smpl_handle.get());

    return smpl_handle;
}
            
texture_handle vk_dynamic_rhi::createTexture(texture* tex)
{
    assert(tex);

    texture_handle tex_handle(tex->getHash());
    auto it = _textures.find(tex_handle.get());
    if (it != _textures.end()) {
        return tex_handle;
    }

    auto pair = _textures.emplace(tex_handle.get(), vk_texture());
    assert(pair.second);

    auto textureDSL = findDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shader_binding_stage::FRAGMENT);
    assert(textureDSL != VK_NULL_HANDLE);

    vk_texture& vktex = pair.first->second;
    vktex.init(tex, _deviceHandle, _device->getGPU(), textureDSL, _descriptorPool);
    
    std::printf("Texture %u created successfully\n", tex_handle.get());

    return tex_handle;
}

void vk_dynamic_rhi::loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex)
{
    auto it = _textures.find(tex_handle.get());
    if (it == _textures.end()) {
        // error;
        return;
    }

    auto& vktex = it->second;

    transitionImageLayout(vktex.texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(vktex.stgBuffer, vktex.texImage, vktex.texExtent.width, vktex.texExtent.height);
    transitionImageLayout(vktex.texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vktex.free_staging();

    std::printf("Texture %u uploaded successfully\n", tex_handle.get());
}

uniform_buffer_handle vk_dynamic_rhi::createUniformBuffer(size_t bufferSize, shader_binding_stage stage)
{
    auto bufferDSL = findDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
    assert(bufferDSL != VK_NULL_HANDLE);

    vk_uniform_buffer& buffer = _uniformBuffers.emplace_back();
    buffer.init(_deviceHandle, _device->getGPU(), bufferDSL, _descriptorPool, bufferSize);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));
    std::printf("Uniform buffer %d created successfully\n", ub_handle.get());

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
        sourceStage,
        destinationStage,
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

VkDescriptorSetLayout vk_dynamic_rhi::findDescriptor(VkDescriptorType in_type, shader_binding_stage in_stage) const
{
    auto it = std::find_if(_descriptors.begin(), _descriptors.end(), [in_type, in_stage](const vk_descriptor& desc)
    {
        return desc.type == in_type && desc.stage == in_stage;
    });
    if (it != _descriptors.end())
    {
        return it->descriptorSetLayout;
    }
    return VK_NULL_HANDLE;
}
