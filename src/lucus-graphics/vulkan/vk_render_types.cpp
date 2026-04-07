#include "vk_render_types.hpp"

#include "vk_utils.hpp"
#include "mesh.hpp"

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


void vk_render_pass::init(VkDevice device, VkFormat inColorFormat, VkFormat inDepthFormat)
{
    _device = device;
    colorFormat = inColorFormat;
    depthFormat = inDepthFormat;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
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

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
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

    std::printf("Render pass created successfully\n");
}

void vk_render_pass::cleanup()
{
    if (renderPass != VK_NULL_HANDLE) { 
        vkDestroyRenderPass(_device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

void vk_image::init(VkDevice device)
{
    _device = device;
}

void vk_image::cleanup()
{
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(_device, image, nullptr);
        image = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(_device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void vk_framebuffer::init(VkDevice device, VkPhysicalDevice gpu, VkRenderPass renderPass, VkExtent2D extent, VkImageView colorImageView, VkFormat depthFormat)
{
    _device = device;

    depth.init(device);
    utils::createImage(device, gpu, extent, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth.image, depth.memory);
    utils::createImageView(device, depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depth.view);

    std::array<VkImageView, 2> attachments = {colorImageView, depth.view};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());;
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void vk_framebuffer::cleanup()
{
    vkDestroyFramebuffer(_device, framebuffer, nullptr);
    depth.cleanup();
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

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

    std::printf("Semaphore and fence are created successfully\n");
}

void vk_frame_sync::cleanup()
{
    vkDestroyFence(_device, fence, nullptr);
    vkDestroySemaphore(_device, imageAvailable, nullptr);
    vkDestroySemaphore(_device, renderFinished, nullptr);
}


void vk_mesh::init(VkDevice device, VkPhysicalDevice gpu, mesh* msh)
{
    assert(msh != nullptr);

    vertexCount = msh->getDrawCount();

    const auto vtxCount = msh->getVertices().size();
    const auto idxCount = msh->getIndices().size();

    bHasVertexData = vtxCount > 0;
    if (bHasVertexData)
    {
        // TODO: Consider staging buffer for larger meshes
        vertexBuffer.init(
            device,
            gpu,
            sizeof(vertex) * vtxCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vertexBuffer.map();
        vertexBuffer.write(msh->getVertices().data(), sizeof(vertex) * vtxCount);

        if (idxCount > 0) {
            indexCount = idxCount;
            indexBuffer.init(
                device,
                gpu,
                sizeof(uint32_t) * idxCount,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            indexBuffer.map();
            indexBuffer.write(msh->getIndices().data(), sizeof(uint32_t) * idxCount);
        }
    }
}

void vk_mesh::cleanup()
{
    vertexBuffer.cleanup();
    indexBuffer.cleanup();
}