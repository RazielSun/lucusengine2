#pragma once

#include "vk_pch.hpp"

namespace lucus
{
    // interface
    class idevice
    {
        public:
            virtual ~idevice() = default;
    };

    class vk_device : public idevice
    {
        public:
            vk_device(VkPhysicalDevice gpu);
            virtual ~vk_device() override;

            VkDevice getDevice() const { return _device; }
            VkCommandPool getCommandPool() const { return _commandPool; }

            struct
            {
                uint32_t graphics;
                uint32_t compute;
                uint32_t transfer;
            } queueFamilyIndices;

            void createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
            
        protected:
            VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

            uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;
            bool extensionSupported(std::string extension) const;

        private:
            VkPhysicalDevice _gpu{ VK_NULL_HANDLE };
            VkPhysicalDeviceProperties _properties{};
            VkPhysicalDeviceFeatures _features{};
            VkPhysicalDeviceFeatures _enabledFeatures{};
            VkPhysicalDeviceMemoryProperties _memoryProperties{};
            std::vector<VkQueueFamilyProperties> _queueFamilyProperties{};
            std::vector<std::string> _supportedExtensions{};

            // Logical Device
            VkDevice _device;
            
            // A Command Pool is a data structure that allows you to create command buffers.
            VkCommandPool _commandPool{ VK_NULL_HANDLE };
    };
}