#include "vk_device.hpp"

#include <algorithm>
#include <cstring>

using namespace lucus;

vk_device::vk_device(VkPhysicalDevice gpu)
    : _gpu(gpu)
{
    assert(_gpu != VK_NULL_HANDLE);

    vkGetPhysicalDeviceProperties(_gpu, &_properties);
	vkGetPhysicalDeviceFeatures(_gpu, &_features);
	vkGetPhysicalDeviceMemoryProperties(_gpu, &_memoryProperties);

    std::printf("Selected GPU: %s\n", _properties.deviceName);

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(_gpu, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    _queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_gpu, &queueFamilyCount, _queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(_gpu, nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(_gpu, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (auto& ext : extensions)
            {
                _supportedExtensions.push_back(ext.extensionName);
            }
        }
    }
}

vk_device::~vk_device()
{
    if (_device)
    {
        vkDestroyDevice(_device, nullptr);
    }
}

void vk_device::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    const float defaultQueuePriority(0.0f);

    // Graphics queue
    if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
    {
        queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        VkDeviceQueueCreateInfo queueInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndices.graphics,
            .queueCount = 1,
            .pQueuePriorities = &defaultQueuePriority
        };
        queueCreateInfos.push_back(queueInfo);
    }
    else
    {
        queueFamilyIndices.graphics = 0;
    }

    // Dedicated compute queue
    if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
    {
        queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
        if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
        {
            // If compute family index differs, we need an additional queue create info for the compute queue
            VkDeviceQueueCreateInfo queueInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamilyIndices.compute,
                .queueCount = 1,
                .pQueuePriorities = &defaultQueuePriority,
            };
            queueCreateInfos.push_back(queueInfo);
        }
    }
    else
    {
        // Else we use the same queue
        queueFamilyIndices.compute = queueFamilyIndices.graphics;
    }

    // Create the logical device representation
    std::vector<const char*> deviceExtensions(enabledExtensions);
    if (useSwapChain)
    {
        // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
    };

    // Descriptor indexing (bindless): query support, then enable via VkPhysicalDeviceFeatures2 chain.
    VkPhysicalDeviceDescriptorIndexingFeatures supportedIndexing{};
    supportedIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceFeatures2 supportedFeatures2{};
    supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supportedFeatures2.pNext = &supportedIndexing;

    vkGetPhysicalDeviceFeatures2(_gpu, &supportedFeatures2);

    std::printf("VkPhysicalDeviceDescriptorIndexingFeatures (physical device, bindless-related):\n");
    std::printf("  shaderSampledImageArrayNonUniformIndexing: %u\n",
                supportedIndexing.shaderSampledImageArrayNonUniformIndexing);
    std::printf("  runtimeDescriptorArray: %u\n", supportedIndexing.runtimeDescriptorArray);
    std::printf("  descriptorBindingPartiallyBound: %u\n", supportedIndexing.descriptorBindingPartiallyBound);
    std::printf("  descriptorBindingSampledImageUpdateAfterBind: %u\n",
                supportedIndexing.descriptorBindingSampledImageUpdateAfterBind);

    VkPhysicalDeviceDescriptorIndexingFeatures enabledIndexing{};
    enabledIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    if (supportedIndexing.shaderSampledImageArrayNonUniformIndexing) {
        enabledIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    }
    if (supportedIndexing.runtimeDescriptorArray) {
        enabledIndexing.runtimeDescriptorArray = VK_TRUE;
    }
    if (supportedIndexing.descriptorBindingPartiallyBound) {
        enabledIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    }
    if (supportedIndexing.descriptorBindingSampledImageUpdateAfterBind) {
        enabledIndexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    }
    if (supportedIndexing.descriptorBindingVariableDescriptorCount) {
        enabledIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
    }

    const bool descriptorIndexingEnabled =
        enabledIndexing.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
        || enabledIndexing.runtimeDescriptorArray == VK_TRUE
        || enabledIndexing.descriptorBindingPartiallyBound == VK_TRUE
        || enabledIndexing.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE
        || enabledIndexing.descriptorBindingVariableDescriptorCount == VK_TRUE;

    if (descriptorIndexingEnabled && extensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
    {
        const char* extName = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
        if (std::find_if(deviceExtensions.begin(), deviceExtensions.end(),
                [extName](const char* e) { return e && std::strcmp(e, extName) == 0; })
            == deviceExtensions.end())
        {
            deviceExtensions.push_back(extName);
        }
    }
    else if (descriptorIndexingEnabled)
    {
        std::cerr << "VK_EXT_descriptor_indexing not listed for device; "
                     "UPDATE_AFTER_BIND bindless layouts may fail validation without Vulkan 1.2 descriptorIndexing.\n";
    }

    if (deviceExtensions.size() > 0)
    {
        for (const char* enabledExtension : deviceExtensions)
        {
            if (!extensionSupported(enabledExtension)) {
                std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
            }
        }

        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = enabledFeatures;
    deviceFeatures2.pNext = &enabledIndexing;

    deviceCreateInfo.pNext = &deviceFeatures2;
    deviceCreateInfo.pEnabledFeatures = nullptr;

    // TODO: future feature
    deviceCreateInfo.enabledLayerCount = 0;

    if (vkCreateDevice(_gpu, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    std::printf("VkDevice created successfully\n");

    // Create a default command pool for graphics command buffers
	// _commandPool = createCommandPool(queueFamilyIndices.graphics);

    // std::printf("VkCommandPool created successfully\n");
}

VkCommandPool vk_device::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = createFlags,
        .queueFamilyIndex = queueFamilyIndex
    };
    VkCommandPool cmdPool;
    if (vkCreateCommandPool(_device, &cmdPoolInfo, nullptr, &cmdPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
    return cmdPool;
}

/**
* Get the index of a queue family that supports the requested queue flags
* SRS - support VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
*/
uint32_t vk_device::getQueueFamilyIndex(VkQueueFlags queueFlags) const
{
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
        {
            if ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
            {
                return i;
            }
        }
    }

    // Dedicated queue for transfer
    // Try to find a queue family index that supports transfer but not graphics and compute
    if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
        {
            if ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
            {
                return i;
            }
        }
    }

    // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
    for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++)
    {
        if ((_queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
        {
            return i;
        }
    }

    throw std::runtime_error("Could not find a matching queue family index");
}

/**
* Check if an extension is supported by the (physical device)
*/
bool vk_device::extensionSupported(std::string extension) const
{
    return (std::find(_supportedExtensions.begin(), _supportedExtensions.end(), extension) != _supportedExtensions.end());
}