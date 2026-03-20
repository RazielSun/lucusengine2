#pragma once

#include "vk_pch.hpp"

#include "vk_viewport.hpp"

namespace lucus
{
    struct vk_render_pass
    {
        vk_render_pass() = default;
        vk_render_pass(VkFormat format) : colorFormat(format) {}

        void init(VkDevice device);
        void cleanup(VkDevice device);

        VkFormat colorFormat{};
        VkRenderPass renderPass{ VK_NULL_HANDLE };
    };

    struct vk_framebuffer
    {
        std::vector<VkFramebuffer> frameBuffers;

        void init(VkDevice device, const vk_viewport& viewport, VkRenderPass renderPass);
        void cleanup(VkDevice device);
    };
}