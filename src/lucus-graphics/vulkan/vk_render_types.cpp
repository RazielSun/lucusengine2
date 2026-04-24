#include "vk_render_types.hpp"

#include "vk_utils.hpp"
#include "mesh.hpp"
#include "texture.hpp"

using namespace lucus;

void vk_commandbuffer_pool::init(VkDevice device, uint32_t queueFamilyIndex)
{
    _device = device;

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    if (vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    allocateCommandBuffers();
}

void vk_commandbuffer_pool::cleanup()
{
    destroyCommandBuffers();
    if (_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(_device, _commandPool, nullptr);
    }
}

VkCommandBuffer& vk_commandbuffer_pool::getCommandBuffer(uint32_t index)
{
    return _buffers[index];
}

void vk_commandbuffer_pool::allocateCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(_buffers.size()),
    };

    if (vkAllocateCommandBuffers(_device, &allocInfo, _buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    std::printf("Command buffer created successfully\n");
}

void vk_commandbuffer_pool::destroyCommandBuffers()
{
    vkFreeCommandBuffers(_device, _commandPool, static_cast<uint32_t>(_buffers.size()), _buffers.data());
}


void vk_render_pass::init(VkDevice device, const vk_render_pass_desc& init_desc)
{
    name = init_desc.name;

    _device = device;

    std::vector<VkAttachmentDescription> attachments; //  = {colorAttachment, depthAttachment}

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkAttachmentReference colorAttachmentRef{};
    VkAttachmentReference depthAttachmentRef{};

    if (init_desc.bUseColor)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = init_desc.colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // Image layout transitions are driven explicitly by IMAGE_BARRIER commands.
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorAttachmentRef.attachment = static_cast<uint32_t>(attachments.size());
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        attachments.push_back(colorAttachment);
    }
    else
    {
        subpass.colorAttachmentCount = 0;
        subpass.pColorAttachments = nullptr;
    }

    if (init_desc.bUseDepth)
    {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = init_desc.depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = name == render_pass_name::SHADOW_PASS ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = static_cast<uint32_t>(attachments.size());
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        attachments.push_back(depthAttachment);
    }
    else
    {
        subpass.pDepthStencilAttachment = nullptr;
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // TODO: change here depends on Color/Depth
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }

    std::printf("Render pass %u created successfully\n", (u32)name);
}

void vk_render_pass::cleanup()
{
    if (renderPass != VK_NULL_HANDLE) { 
        vkDestroyRenderPass(_device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

void vk_images::init(VkDevice device, VkPhysicalDevice gpu, const vk_images_desc& init_desc)
{
    _device = device;

    if (!bPreinitialized)
    {
        images.resize(init_desc.count);
        memories.resize(init_desc.count);
    }
    views.resize(init_desc.count);
    if (init_desc.bUseDescriptorSet)
    {
        descriptorSets.resize(init_desc.count);
    }

    for (u32 i = 0; i < init_desc.count; ++i)
    {
        if (!bPreinitialized)
        {
            utils::createImage(device, gpu, init_desc.extent, init_desc.format, init_desc.usage, init_desc.properties, images[i], memories[i]);
        }
        utils::createImageView(device, images[i], init_desc.format, init_desc.aspectFlags, views[i]);

        if (init_desc.bUseDescriptorSet)
        {
            assert(init_desc.descriptorPool);
            assert(init_desc.descriptorSetLayout);

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = init_desc.descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &init_desc.descriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }
            
            VkDescriptorImageInfo dsInfo{};
            dsInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dsInfo.imageView = views[i];
            dsInfo.sampler = VK_NULL_HANDLE; // because your sampler is a separate descriptor

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSets[i];
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write.descriptorCount = 1;
            write.pImageInfo = &dsInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }
}

void vk_images::cleanup()
{
    for (auto& view : views)
    {
        vkDestroyImageView(_device, view, nullptr);
    }
    views.clear();
    if (!bPreinitialized)
    {
        for (auto& image : images)
        {
            vkDestroyImage(_device, image, nullptr);
        }
        images.clear();
        for (auto& memory : memories)
        {
            vkFreeMemory(_device, memory, nullptr);
        }
        memories.clear();
    }
}

void vk_render_target::init(VkDevice device, VkPhysicalDevice gpu, const vk_render_target_desc& init_desc)
{
    _device = device;

    bColor = init_desc.bUseColor;
    if (bColor)
    {
        vk_images_desc color_desc
        {
            .count = init_desc.count,
            .format = init_desc.colorFormat,
            .extent = init_desc.extent,
            // TODO ?
            // .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            // .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            .bUseDescriptorSet = init_desc.bUseColorDescriptorSet,
            .descriptorPool = init_desc.descriptorPool,
            .descriptorSetLayout = init_desc.descriptorSetLayout,
        };
        color.init(device, gpu, color_desc);
    }

    bDepth = init_desc.bUseDepth;
    if (bDepth)
    {
        vk_images_desc depth_desc
        {
            .count = init_desc.count,
            .format = init_desc.depthFormat,
            .extent = init_desc.extent,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .aspectFlags =VK_IMAGE_ASPECT_DEPTH_BIT,
            .bUseDescriptorSet = init_desc.bUseDepthDescriptorSet,
            .descriptorPool = init_desc.descriptorPool,
            .descriptorSetLayout = init_desc.descriptorSetLayout,
        };
        depth.init(device, gpu, depth_desc);
    }

    bFramebuffer = init_desc.bUseFramebuffer;
    if (bFramebuffer)
    {
        assert(init_desc.renderPass);
        
        frameBuffers.resize(init_desc.count);

        for (u32 i = 0; i < init_desc.count; ++i)
        {
            std::vector<VkImageView> attachments;
            if (bColor) attachments.push_back(color.views[i]);
            if (bDepth) attachments.push_back(depth.views[i]);

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = init_desc.renderPass;
            framebufferInfo.attachmentCount = static_cast<u32>(attachments.size());;
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = init_desc.extent.width;
            framebufferInfo.height = init_desc.extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }
}

void vk_render_target::cleanup()
{
    for(auto& framebuffer : frameBuffers)
    {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }
    depth.cleanup();
    color.cleanup();
}

void vk_image_sync::init(VkDevice device)
{
    _device = device;

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    std::printf("Semaphores are created successfully\n");
}

void vk_image_sync::cleanup()
{
    vkDestroySemaphore(_device, imageAvailable, nullptr);
    vkDestroySemaphore(_device, renderFinished, nullptr);
}

void vk_frame_sync::init(VkDevice device)
{
    _device = device;

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    std::printf("Fences are created successfully\n");
}

void vk_frame_sync::cleanup()
{
    vkDestroyFence(_device, fence, nullptr);
}


void vk_mesh::init(VkDevice device, VkPhysicalDevice gpu, mesh* msh)
{
    assert(msh != nullptr);

    vertexCount = msh->getVertexCount();
    const auto idxCount = msh->getIndices().size();

    if (msh->hasVertices())
    {
        // TODO: Consider staging buffer for larger meshes
        vertexBuffer.init(
            device,
            gpu,
            sizeof(vertex) * vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vertexBuffer.map();
        vertexBuffer.write(msh->getVertices().data(), sizeof(vertex) * vertexCount);

        if (idxCount > 0)
        {
            indexCount = idxCount;
            indexBuffer.init(
                device,
                gpu,
                sizeof(u32) * idxCount,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            indexBuffer.map();
            indexBuffer.write(msh->getIndices().data(), sizeof(u32) * idxCount);
        }
    }
}

void vk_mesh::cleanup()
{
    vertexBuffer.cleanup();
    indexBuffer.cleanup();
}

void vk_texture::init(texture* tex, VkDevice device, VkPhysicalDevice gpu, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool)
{
    assert(tex);
    assert(device);
    assert(gpu);

    _device = device;

    utils::createTextureImage(tex, device, gpu, stgBuffer, stgBufferMemory, texImage, texImageMemory, texSize, texExtent);
    utils::createImageView(device, texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, texImageView);

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        std::printf("Texture initDescriptor called successfully\n");
    }
    
    {
        std::array<VkWriteDescriptorSet, 1> write{};

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texImageView;

        // TEXTURE
        write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[0].dstSet = descriptorSet;
        write[0].dstBinding = 0;
        write[0].dstArrayElement = 0;
        write[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write[0].descriptorCount = 1;
        write[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(write.size()), write.data(), 0, nullptr);

        std::printf("Texture updateDescriptor called successfully\n");
    }
}

void vk_texture::free_staging()
{
    vkDestroyBuffer(_device, stgBuffer, nullptr);
    vkFreeMemory(_device, stgBufferMemory, nullptr);
}

void vk_texture::cleanup()
{
    vkDestroyImageView(_device, texImageView, nullptr);
    vkDestroyImage(_device, texImage, nullptr);
    vkFreeMemory(_device, texImageMemory, nullptr);
}

void vk_sampler::init(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool)
{
    _device = device;

    // TODO: Create in the feature CLAMP / NON LINEAR

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        std::printf("Texture initDescriptor called successfully\n");
    }

    {
        std::array<VkWriteDescriptorSet, 1> write{};

        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = sampler;

        // SAMPLER
        write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[0].dstSet = descriptorSet;
        write[0].dstBinding = 0;
        write[0].dstArrayElement = 0;
        write[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write[0].descriptorCount = 1;
        write[0].pImageInfo = &samplerInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(write.size()), write.data(), 0, nullptr);

        std::printf("Sampler updateDescriptor called successfully\n");
    }

    std::printf("Sampler created successfully\n");
}

void vk_sampler::cleanup()
{
    vkDestroySampler(_device, sampler, nullptr);
}

void vk_descriptor::init(VkDevice device, VkDescriptorType in_type, shader_binding_stage in_stage)
{
    _device = device;
    type = in_type;
    stage = in_stage;

    VkShaderStageFlags flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // BOTH
    if (stage == shader_binding_stage::VERTEX)
    {
        flags = VK_SHADER_STAGE_VERTEX_BIT;
    }
    else if (stage == shader_binding_stage::FRAGMENT)
    {
        flags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    std::array<VkDescriptorSetLayoutBinding, 1> bindings{};
    bindings[0] = VkDescriptorSetLayoutBinding
    {
        .binding = 0,
        .descriptorType = type, // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = flags,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    std::printf("Descriptor Set Layout [stage: %d type: %d] created successfully\n", (u32)stage, (u32)type);
}

void vk_descriptor::cleanup()
{
    vkDestroyDescriptorSetLayout(_device, descriptorSetLayout, nullptr);
}
