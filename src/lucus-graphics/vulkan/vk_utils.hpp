#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    namespace utils
    {
        void createBuffer(VkDevice device, VkPhysicalDevice gpu, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void createImage(VkDevice device, VkPhysicalDevice gpu, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
        void createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& view);
        void createTextureImage(const std::string& fileName, VkDevice device, VkPhysicalDevice gpu, VkBuffer& stgBuffer, VkDeviceMemory& stgBufferMemory, VkImage& texImage, VkDeviceMemory& texImageMemory, VkDeviceSize& texSize, VkExtent2D& texExtent);
        
        uint32_t findMemoryType(VkPhysicalDevice gpu, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkFormat findSupportedFormat(VkPhysicalDevice gpu, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkFormat findDepthFormat(VkPhysicalDevice gpu);
        bool hasStencilComponent(VkFormat format);
    }
}