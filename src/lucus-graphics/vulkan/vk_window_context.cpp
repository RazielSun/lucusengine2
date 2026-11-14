#include "vk_window_context.hpp"

#include "window.hpp"
#include "vk_utils.hpp"

#include "glfw_include.hpp"

using namespace lucus;

void vk_window_context::init(VkInstance instance, VkPhysicalDevice gpu, VkDevice device, window* window)
{
    assert(gpu);
    assert(device);
    assert(instance);
    assert(window);

    _instance = instance;
    _device = device;
	_gpu = gpu;

    VkResult result = glfwCreateWindowSurface(instance, window->getGLFWwindow(), nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }

    initSurface(gpu, window);
    swapChain.init(gpu, device, surface, window);

	imageSync.resize(swapChain.imageCount);
	for (auto& img_sync : imageSync) {
        img_sync.init(device);
    }
}

void vk_window_context::init_images(vk_render_target& render_target)
{
	render_target.bFromSwapChain = true;

	u32 imageCount = swapChain.imageCount;
	render_target.images.resize(imageCount);
    if (vkGetSwapchainImagesKHR(_device, swapChain.swapChain, &imageCount, render_target.images.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get swapchain images!");
    }
    assert(imageCount == swapChain.imageCount);

	render_target.desc.bUseDescriptorSet = false;
	render_target.createResources();
}

void vk_window_context::cleanup()
{
    for (auto& img_sync : imageSync) {
        img_sync.cleanup();
    }

    swapChain.cleanup();

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_instance, surface, nullptr);
    }
}

void vk_window_context::initSurface(VkPhysicalDevice gpu, window* window)
{
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, nullptr);
	assert(queueCount >= 1);
	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queueProps.data());

	std::vector<VkBool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++) 
	{
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supportsPresent[i]);
	}

	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++) {
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)  {
			if (graphicsQueueNodeIndex == UINT32_MAX) {
				graphicsQueueNodeIndex = i;
			}
			if (supportsPresent[i] == VK_TRUE) {
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	if (presentQueueNodeIndex == UINT32_MAX) 
	{	
		for (uint32_t i = 0; i < queueCount; ++i) {
			if (supportsPresent[i] == VK_TRUE) {
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)  {
		throw std::runtime_error("Could not find a graphics and/or presenting queue!");
	}
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
		throw std::runtime_error("Separate graphics and presenting queues are not supported yet!");
	}

	uint32_t formatCount;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, NULL) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve surface formats!");
    }
	assert(formatCount > 0);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve surface formats!");
    }

	selectedFormat = surfaceFormats[0];
	std::vector<VkFormat> preferredImageFormats = { 
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM, 
		VK_FORMAT_A8B8G8R8_UNORM_PACK32 
	};
	for (auto& availableFormat : surfaceFormats) {
		if (std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end()) {
			selectedFormat = availableFormat;
			break;
		}
	}

	swapChain.colorFormat = selectedFormat.format;
	swapChain.colorSpace = selectedFormat.colorSpace;

    std::printf("VkSurfaceKHR created successfully\n");
}

void vk_window_context::acquire_image(u32 frameIndex)
{
	u32 nextImageIndex = frameIndex % static_cast<u32>(imageSync.size());
    const VkResult result = vkAcquireNextImageKHR(_device, swapChain.swapChain, UINT64_MAX,
        imageSync[nextImageIndex].imageAvailable, VK_NULL_HANDLE, &currentImageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Vulkan acquire failed with result " + std::to_string(result));
    }
}

void vk_swapchain::init(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface, window* window)
{
    _device = device;

	VkSwapchainKHR oldSwapchain = swapChain;

	VkSurfaceCapabilitiesKHR capabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities) != VK_SUCCESS) {
		throw std::runtime_error("Failed to get surface capabilities!");
	}

    int width = window->framebuffer_width();
    int height = window->framebuffer_height();

	if (capabilities.currentExtent.width == (uint32_t)-1) {
		extent.width = width;
		extent.height = height;
	} else {
		extent = capabilities.currentExtent;
		width = capabilities.currentExtent.width;
		height = capabilities.currentExtent.height;
	}

	uint32_t presentModeCount;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get surface present modes!");
    }
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get surface present modes!");
    }

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchainPresentMode = availablePresentMode;
            break;
        }
    }

    uint32_t desiredNumberOfSwapchainImages = g_desiredSwapchainImageCount;
	if (desiredNumberOfSwapchainImages < capabilities.minImageCount) {
		desiredNumberOfSwapchainImages = capabilities.minImageCount;
	} else if (capabilities.maxImageCount > 0 && desiredNumberOfSwapchainImages > capabilities.maxImageCount) {
		desiredNumberOfSwapchainImages = capabilities.maxImageCount;
	}

	VkSurfaceTransformFlagsKHR preTransform;
	if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		preTransform = capabilities.currentTransform;
	}

    imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkSwapchainCreateInfoKHR createSwapchainInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = desiredNumberOfSwapchainImages,
        .imageFormat = colorFormat,
        .imageColorSpace = colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = imageUsage,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = swapchainPresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapchain,
    };

    // TODO

    if (vkCreateSwapchainKHR(device, &createSwapchainInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    // The driver decides the real count; minImageCount above was only a request.
    if (vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to get swapchain image count!");
    }
    if (imageCount == 0) {
        throw std::runtime_error("Swapchain reported zero images!");
    }

	std::printf("VkSwapchainKHR created successfully (%dx%d, %u images, %u requested)\n",
        extent.width, extent.height, imageCount, desiredNumberOfSwapchainImages);
}

void vk_swapchain::cleanup()
{
    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, swapChain, nullptr);
    }
}
