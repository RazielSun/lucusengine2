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

	for (auto& img_sync : imageSync) {
        img_sync.init(device);
    }
}

void vk_window_context::init_images(vk_render_target& render_target)
{
	render_target.bFromSwapChain = true;

	u32 imageCount = swapChain.imageCount;
	render_target.images.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, swapChain.swapChain, &imageCount, render_target.images.data());

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
    // Get available queue family properties
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, nullptr);
	assert(queueCount >= 1);
	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<VkBool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++) 
	{
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supportsPresent[i]);
	}

    // Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
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
		// If there's no queue that supports both present and graphics
		// try to find a separate present queue
		for (uint32_t i = 0; i < queueCount; ++i) {
			if (supportsPresent[i] == VK_TRUE) {
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

    // Exit if either a graphics or a presenting queue hasn't been found
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)  {
		throw std::runtime_error("Could not find a graphics and/or presenting queue!");
	}
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
		throw std::runtime_error("Separate graphics and presenting queues are not supported yet!");
	}
	// queueNodeIndex = graphicsQueueNodeIndex;

    // Get list of supported surface formats
	uint32_t formatCount;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, NULL) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve surface formats!");
    }
	assert(formatCount > 0);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to retrieve surface formats!");
    }

	// We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
	// Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
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
	u32 nextImageIndex = frameIndex % g_swapchainImageCount;
    vkAcquireNextImageKHR(_device, swapChain.swapChain, UINT64_MAX, imageSync[nextImageIndex].imageAvailable, (VkFence)nullptr, &currentImageIndex);
}

void vk_swapchain::init(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface, window* window)
{
    _device = device;

	// Store the current swap chain handle so we can use it later on to ease up recreation
	VkSwapchainKHR oldSwapchain = swapChain;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR capabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities) != VK_SUCCESS) {
		throw std::runtime_error("Failed to get surface capabilities!");
	}

    int width = window->framebuffer_width();
    int height = window->framebuffer_height();

	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (capabilities.currentExtent.width == (uint32_t)-1) {
		// If the surface size is undefined, the size is set to the size of the images requested
		extent.width = width;
		extent.height = height;
	} else {
		// If the surface size is defined, the swap chain size must match
		extent = capabilities.currentExtent;
		width = capabilities.currentExtent.width;
		height = capabilities.currentExtent.height;
	}

    // Select a present mode for the swapchain
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
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchainPresentMode = availablePresentMode;
            break;
        }
    }

	// The FIFO present mode is the only mode that is guaranteed to be available
	// and supports triple buffering, so we prefer to use triple buffering if it's available
    uint32_t desiredNumberOfSwapchainImages = g_swapchainImageCount;
	if (desiredNumberOfSwapchainImages < capabilities.minImageCount) {
		desiredNumberOfSwapchainImages = capabilities.minImageCount;
	} else if (capabilities.maxImageCount > 0 && desiredNumberOfSwapchainImages > capabilities.maxImageCount) {
		desiredNumberOfSwapchainImages = capabilities.maxImageCount;
	}

    // Find the transformation of the surface
	VkSurfaceTransformFlagsKHR preTransform;
	if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		// We prefer a non-rotated transform
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		preTransform = capabilities.currentTransform;
	}

    // TODO
    // // Find a supported composite alpha format (not all devices support alpha opaque)
	// VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// // Simply select the first composite alpha format available
	// std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
	// 	VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
	// 	VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
	// 	VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
	// 	VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	// };
	// for (auto& compositeAlphaFlag : compositeAlphaFlags) {
	// 	if (surfaceCaps.supportedCompositeAlpha & compositeAlphaFlag) {
	// 		compositeAlpha = compositeAlphaFlag;
	// 		break;
	// 	};
	// }

    VkSwapchainCreateInfoKHR createSwapchainInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = desiredNumberOfSwapchainImages,
        .imageFormat = colorFormat,
        .imageColorSpace = colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = swapchainPresentMode,
        // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.clipped = VK_TRUE,
		// Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
		.oldSwapchain = oldSwapchain,
    };

    // TODO
    // QueueFamilyIndices indices = findQueueFamilies(_physicalDevice, _surface);
    // uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // if (indices.graphicsFamily != indices.presentFamily) {
    //     createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    //     createInfo.queueFamilyIndexCount = 2;
    //     createInfo.pQueueFamilyIndices = queueFamilyIndices;
    // } else {
        // createSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // createSwapchainInfo.queueFamilyIndexCount = 0; // Optional
        // createSwapchainInfo.pQueueFamilyIndices = nullptr; // Optional
    // }

    if (vkCreateSwapchainKHR(device, &createSwapchainInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);

	std::printf("VkSwapchainKHR created successfully (%dx%d)\n", extent.width, extent.height);
}

void vk_swapchain::cleanup()
{
    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, swapChain, nullptr);
    }
}