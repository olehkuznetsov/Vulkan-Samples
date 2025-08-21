/* Copyright (c) 2018-2025, Arm Limited and Contributors
 * Copyright (c) 2025, Sascha Willems
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cmath>
#include <cstring>
#include <string>
#include <android/trace.h>

#include "hello_many_triangles.h"

#include "common/vk_common.h"
#include "core/util/logging.hpp"
#include "filesystem/legacy.h"
#include "platform/window.h"

/**
 * @brief Validates a list of required extensions, comparing it with the available ones.
 *
 * @param required A vector containing required extension names.
 * @param available A VkExtensionProperties object containing available extensions.
 * @return true if all required extensions are available
 * @return false otherwise
 */
bool HelloManyTriangles::validate_extensions(const std::vector<const char *>          &required,
                                        const std::vector<VkExtensionProperties> &available)
{
	for (auto extension : required)
	{
		bool found = false;
		for (auto &available_extension : available)
		{
			if (strcmp(available_extension.extensionName, extension) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			return false;
		}
	}

	return true;
}

/**
 * @brief Initializes the Vulkan instance.
 */
void HelloManyTriangles::init_instance()
{
	LOGI("Initializing vulkan instance.");

	if (volkInitialize())
	{
		throw std::runtime_error("Failed to initialize volk.");
	}

	uint32_t instance_extension_count;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr));

	std::vector<VkExtensionProperties> available_instance_extensions(instance_extension_count);
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, available_instance_extensions.data()));

	std::vector<const char *> required_instance_extensions{VK_KHR_SURFACE_EXTENSION_NAME};

	// Validation layers help finding wrong api usage, we enable them when explicitly requested or in debug builds
	// For this we use the debug utils extension if it is supported
	bool has_debug_utils = false;
	for (const auto &ext : available_instance_extensions)
	{
		if (strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
		{
			has_debug_utils = true;
			debug_ext_enabled = true;
			required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			break;
		}
	}
	if (!has_debug_utils)
	{
		LOGW("{} not supported or available", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		LOGW("Make sure to compile the sample in debug mode and/or enable the validation layers");
	}

#if (defined(VKB_ENABLE_PORTABILITY))
	required_instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	bool portability_enumeration_available = false;
	if (std::ranges::any_of(available_instance_extensions,
	                        [](VkExtensionProperties const &extension) { return strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0; }))
	{
		required_instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		portability_enumeration_available = true;
	}
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	required_instance_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	required_instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	required_instance_extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	required_instance_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	required_instance_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	required_instance_extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DISPLAY_KHR)
	required_instance_extensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#else
#	pragma error Platform not supported
#endif

	if (!validate_extensions(required_instance_extensions, available_instance_extensions))
	{
		throw std::runtime_error("Required instance extensions are missing.");
	}

	std::vector<const char *> requested_instance_layers{};

#if defined(VKB_DEBUG) || defined(VKB_VALIDATION_LAYERS)
	char const *validationLayer = "VK_LAYER_KHRONOS_validation";

	uint32_t instance_layer_count;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

	std::vector<VkLayerProperties> supported_instance_layers(instance_layer_count);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_instance_layers.data()));

	if (std::ranges::any_of(supported_instance_layers, [&validationLayer](auto const &lp) { return strcmp(lp.layerName, validationLayer) == 0; }))
	{
		requested_instance_layers.push_back(validationLayer);
		LOGI("Enabled Validation Layer {}", validationLayer);
	}
	else
	{
		LOGW("Validation Layer {} is not available", validationLayer);
	}
#endif

	VkApplicationInfo app{
	    .sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	    .pApplicationName = "Hello Triangle",
	    .pEngineName      = "Vulkan Samples",
	    .apiVersion       = VK_API_VERSION_1_1};

	VkInstanceCreateInfo instance_info{
	    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	    .pApplicationInfo        = &app,
	    .enabledLayerCount       = static_cast<uint32_t>(requested_instance_layers.size()),
	    .ppEnabledLayerNames     = requested_instance_layers.data(),
	    .enabledExtensionCount   = static_cast<uint32_t>(required_instance_extensions.size()),
	    .ppEnabledExtensionNames = required_instance_extensions.data()};

#if (defined(VKB_ENABLE_PORTABILITY))
	if (portability_enumeration_available)
	{
		instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
#endif

	// Create the Vulkan instance
	VK_CHECK(vkCreateInstance(&instance_info, nullptr, &context.instance));

	volkLoadInstance(context.instance);
}

/**
 * @brief Initializes the Vulkan physical device and logical device.
 */
void HelloManyTriangles::init_device()
{
	LOGI("Initializing vulkan device.");

	uint32_t gpu_count = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(context.instance, &gpu_count, nullptr));

	if (gpu_count < 1)
	{
		throw std::runtime_error("No physical device found.");
	}

	// For simplicity, the sample selects the first gpu that has a graphics and present queue
	std::vector<VkPhysicalDevice> gpus(gpu_count);
	VK_CHECK(vkEnumeratePhysicalDevices(context.instance, &gpu_count, gpus.data()));

	for (size_t i = 0; i < gpu_count && (context.graphics_queue_index < 0); i++)
	{
		context.gpu = gpus[i];

		uint32_t queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &queue_family_count, nullptr);

		if (queue_family_count < 1)
		{
			throw std::runtime_error("No queue family found.");
		}

		std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &queue_family_count, queue_family_properties.data());

		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			VkBool32 supports_present;
			vkGetPhysicalDeviceSurfaceSupportKHR(context.gpu, i, context.surface, &supports_present);

			// Find a queue family which supports graphics and presentation.
			if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_present)
			{
				context.graphics_queue_index = i;
				break;
			}
		}
	}

	if (context.graphics_queue_index < 0)
	{
		throw std::runtime_error("Did not find suitable device with a queue that supports graphics and presentation.");
	}

	uint32_t device_extension_count;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &device_extension_count, nullptr));
	std::vector<VkExtensionProperties> device_extensions(device_extension_count);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &device_extension_count, device_extensions.data()));

	// Since this sample has visual output, the device needs to support the swapchain extension
	std::vector<const char *> required_device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
//	required_device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

	if (!validate_extensions(required_device_extensions, device_extensions))
	{
		throw std::runtime_error("Required device extensions are missing.");
	}

#if (defined(VKB_ENABLE_PORTABILITY))
	// VK_KHR_portability_subset must be enabled if present in the implementation (e.g on macOS/iOS with beta extensions enabled)
	if (std::ranges::any_of(device_extensions,
	                        [](VkExtensionProperties const &extension) { return strcmp(extension.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0; }))
	{
		required_device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	}
#endif

	// The sample uses a single graphics queue
	const float queue_priority = 1.0f;

	VkDeviceQueueCreateInfo queue_info{
	    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
	    .queueFamilyIndex = static_cast<uint32_t>(context.graphics_queue_index),
	    .queueCount       = 1,
	    .pQueuePriorities = &queue_priority};

	VkDeviceCreateInfo device_info{
	    .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	    .queueCreateInfoCount    = 1,
	    .pQueueCreateInfos       = &queue_info,
	    .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.size()),
	    .ppEnabledExtensionNames = required_device_extensions.data()};

	VK_CHECK(vkCreateDevice(context.gpu, &device_info, nullptr, &context.device));
	volkLoadDevice(context.device);

	vkGetDeviceQueue(context.device, context.graphics_queue_index, 0, &context.queue);

	// This sample uses the Vulkan Memory Alloctor (VMA), which needs to be set up
	VmaVulkanFunctions vma_vulkan_func{
	    .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
	    .vkGetDeviceProcAddr   = vkGetDeviceProcAddr};

	VmaAllocatorCreateInfo allocator_info{
	    .physicalDevice   = context.gpu,
	    .device           = context.device,
	    .pVulkanFunctions = &vma_vulkan_func,
	    .instance         = context.instance};

	VkResult result = vmaCreateAllocator(&allocator_info, &context.vma_allocator);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create allocator for VMA allocator");
	}
}

/**
 * @brief Initializes per frame data.
 * @param per_frame The data of a frame.
 */
void HelloManyTriangles::init_per_frame(PerFrame &per_frame)
{
	VkFenceCreateInfo info{
	    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	    .flags = VK_FENCE_CREATE_SIGNALED_BIT};
	VK_CHECK(vkCreateFence(context.device, &info, nullptr, &per_frame.queue_submit_fence));

	VkCommandPoolCreateInfo cmd_pool_info{
	    .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	    .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
	    .queueFamilyIndex = static_cast<uint32_t>(context.graphics_queue_index)};
	VK_CHECK(vkCreateCommandPool(context.device, &cmd_pool_info, nullptr, &per_frame.primary_command_pool));
}

/**
 * @brief Tears down the frame data.
 * @param per_frame The data of a frame.
 */
void HelloManyTriangles::teardown_per_frame(PerFrame &per_frame)
{
	if (per_frame.queue_submit_fence != VK_NULL_HANDLE)
	{
		vkDestroyFence(context.device, per_frame.queue_submit_fence, nullptr);

		per_frame.queue_submit_fence = VK_NULL_HANDLE;
	}

	if (per_frame.primary_command_pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(context.device, per_frame.primary_command_pool, nullptr);

		per_frame.primary_command_pool = VK_NULL_HANDLE;
	}

	if (per_frame.swapchain_acquire_semaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(context.device, per_frame.swapchain_acquire_semaphore, nullptr);

		per_frame.swapchain_acquire_semaphore = VK_NULL_HANDLE;
	}

	if (per_frame.swapchain_release_semaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(context.device, per_frame.swapchain_release_semaphore, nullptr);

		per_frame.swapchain_release_semaphore = VK_NULL_HANDLE;
	}
}

/**
 * @brief Initializes the Vulkan swapchain.
 */
void HelloManyTriangles::init_swapchain()
{
	VkSurfaceCapabilitiesKHR surface_properties;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu, context.surface, &surface_properties));

	VkSurfaceFormatKHR format = vkb::select_surface_format(context.gpu, context.surface);

	VkExtent2D swapchain_size{};
	if (surface_properties.currentExtent.width == 0xFFFFFFFF)
	{
		swapchain_size.width  = context.swapchain_dimensions.width;
		swapchain_size.height = context.swapchain_dimensions.height;
	}
	else
	{
		swapchain_size = surface_properties.currentExtent;
	}

	// FIFO must be supported by all implementations.
	VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	// Determine the number of VkImage's to use in the swapchain.
	// Ideally, we desire to own 1 image at a time, the rest of the images can
	// either be rendered to and/or being queued up for display.
	uint32_t desired_swapchain_images = surface_properties.minImageCount + 1;
	if ((surface_properties.maxImageCount > 0) && (desired_swapchain_images > surface_properties.maxImageCount))
	{
		// Application must settle for fewer images than desired.
		desired_swapchain_images = surface_properties.maxImageCount;
	}

	// Figure out a suitable surface transform.
	VkSurfaceTransformFlagBitsKHR pre_transform;
	if (surface_properties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		pre_transform = surface_properties.currentTransform;
	}

	VkSwapchainKHR old_swapchain = context.swapchain;

	// Find a supported composite type.
	VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}
	else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
	{
		composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}
	else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
	{
		composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	}
	else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
	{
		composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	}

	VkSwapchainCreateInfoKHR info{
	    .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
	    .surface          = context.surface,
	    .minImageCount    = desired_swapchain_images,
	    .imageFormat      = format.format,
	    .imageColorSpace  = format.colorSpace,
	    .imageExtent      = swapchain_size,
	    .imageArrayLayers = 1,
	    .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .preTransform     = pre_transform,
	    .compositeAlpha   = composite,
	    .presentMode      = swapchain_present_mode,
	    .clipped          = true,
	    .oldSwapchain     = old_swapchain};

	VK_CHECK(vkCreateSwapchainKHR(context.device, &info, nullptr, &context.swapchain));

	if (old_swapchain != VK_NULL_HANDLE)
	{
		for (VkImageView image_view : context.swapchain_image_views)
		{
			vkDestroyImageView(context.device, image_view, nullptr);
		}

		for (auto &per_frame : context.per_frame)
		{
			teardown_per_frame(per_frame);
		}

		context.swapchain_image_views.clear();

		vkDestroySwapchainKHR(context.device, old_swapchain, nullptr);
	}

	context.swapchain_dimensions = {swapchain_size.width, swapchain_size.height, format.format};

	uint32_t image_count;
	VK_CHECK(vkGetSwapchainImagesKHR(context.device, context.swapchain, &image_count, nullptr));

	/// The swapchain images.
	std::vector<VkImage> swapchain_images(image_count);
	VK_CHECK(vkGetSwapchainImagesKHR(context.device, context.swapchain, &image_count, swapchain_images.data()));

	// Initialize per-frame resources.
	// Every swapchain image has its own command pool and fence manager.
	// This makes it very easy to keep track of when we can reset command buffers and such.
	context.per_frame.clear();
	context.per_frame.resize(image_count);

	for (size_t i = 0; i < image_count; i++)
	{
		init_per_frame(context.per_frame[i]);
	}

	for (size_t i = 0; i < image_count; i++)
	{
		// Create an image view which we can render into.
		VkImageViewCreateInfo view_info{
		    .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		    .image            = swapchain_images[i],
		    .viewType         = VK_IMAGE_VIEW_TYPE_2D,
		    .format           = context.swapchain_dimensions.format,
		    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

		VkImageView image_view;
		VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &image_view));

		context.swapchain_image_views.push_back(image_view);
	}
}

/**
 * @brief Helper function to load a shader module from an offline-compiled SPIR-V file
 * @param path The path for the shader (relative to the assets directory).
 * @returns A VkShaderModule handle. Aborts execution if shader creation fails.
 */
VkShaderModule HelloManyTriangles::load_shader_module(const std::string &path)
{
	auto spirv = vkb::fs::read_shader_binary_u32(path);

	VkShaderModuleCreateInfo module_info{
	    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
	    .codeSize = spirv.size() * sizeof(uint32_t),
	    .pCode    = spirv.data()};

	VkShaderModule shader_module;
	VK_CHECK(vkCreateShaderModule(context.device, &module_info, nullptr, &shader_module));

	return shader_module;
}

/**
 * @brief Adds a render pass that draws a circle.
 * @param offset The 2D offset for the center of the circle.
 * @param scale The radius of the circle.
 * @param triangle_count The number of triangles to use to approximate the circle.
 * @param load_op The load operation for the color attachment.
 * @param final_layout The final layout for the color attachment.
 */
void HelloManyTriangles::add_render_pass(const char* description, const glm::vec2 &offset, float scale, uint32_t triangle_count, VkAttachmentLoadOp load_op, VkImageLayout final_layout)
{
	RenderPassData data;

	data.description = description;

	// Generate circle vertices
	std::vector<Vertex> vertices;
	vertices.reserve(triangle_count * 3);
	for (uint32_t i = 0; i < triangle_count; ++i)
	{
		float angle1 = 2.0f * glm::pi<float>() * static_cast<float>(i) / triangle_count;
		float angle2 = 2.0f * glm::pi<float>() * static_cast<float>(i + 1) / triangle_count;

		// Center vertex
		vertices.push_back({{offset.x, offset.y, 0.5f}, {1.0f, 1.0f, 1.0f}});

		// First vertex on circumference
		vertices.push_back({{offset.x + cos(angle1) * scale, offset.y + sin(angle1) * scale, 0.5f}, {0.5f + 0.5f * cos(angle1), 0.5f + 0.5f * sin(angle1), 0.0f}});

		// Second vertex on circumference
		vertices.push_back({{offset.x + cos(angle2) * scale, offset.y + sin(angle2) * scale, 0.5f}, {0.5f + 0.5f * cos(angle2), 0.5f + 0.5f * sin(angle2), 0.0f}});
	}
	data.vertex_count = vertices.size();

	// Create vertex buffer
	const VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
	VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = buffer_size, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
	VmaAllocationCreateInfo buffer_alloc_ci{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
	VmaAllocationInfo       buffer_alloc_info{};
	vmaCreateBuffer(context.vma_allocator, &buffer_info, &buffer_alloc_ci, &data.vertex_buffer, &data.vertex_buffer_allocation, &buffer_alloc_info);
	if (buffer_alloc_info.pMappedData)
	{
		memcpy(buffer_alloc_info.pMappedData, vertices.data(), buffer_size);
	}
	else
	{
		throw std::runtime_error("Could not map vertex buffer.");
	}

	// Create render pass
	VkAttachmentDescription attachment{
	    .format         = context.swapchain_dimensions.format,
	    .samples        = VK_SAMPLE_COUNT_1_BIT,
	    .loadOp         = load_op,
	    .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
	    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    .initialLayout  = render_pass_data.empty() ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	    .finalLayout    = final_layout};

	VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref};

	VkSubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0, .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

	VkRenderPassCreateInfo rp_info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &attachment, .subpassCount = 1, .pSubpasses = &subpass, .dependencyCount = 1, .pDependencies = &dependency};

	VK_CHECK(vkCreateRenderPass(context.device, &rp_info, nullptr, &data.render_pass));

	if (debug_ext_enabled)
	{
		VkDebugUtilsObjectNameInfoEXT object_name = {};
		object_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		object_name.objectType = VK_OBJECT_TYPE_RENDER_PASS;
		object_name.objectHandle = reinterpret_cast<uint64_t>(data.render_pass);
		object_name.pObjectName = description;
		
		vkSetDebugUtilsObjectNameEXT(context.device, &object_name);

		LOGI("Add render_pass objectHandle {} render_pass: {}", object_name.objectHandle, reinterpret_cast<uint64_t>(data.render_pass));
		LOGI("Add render_pass {} debug info: {}", object_name.objectHandle, description);

	}

	render_pass_data.push_back(data);
}

/**
 * @brief Initializes the Vulkan pipeline.
 */
void HelloManyTriangles::init_pipeline()
{
	// Create a blank pipeline layout.
	// We are not binding any resources to the pipeline in this first sample.
	VkPipelineLayoutCreateInfo layout_info{
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VK_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr, &context.pipeline_layout));

	// The Vertex input properties define the interface between the vertex buffer and the vertex shader.

	// Specify we will use triangle lists to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo input_assembly{
	    .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

	// Define the vertex input binding.
	VkVertexInputBindingDescription binding_description{
	    .binding   = 0,
	    .stride    = sizeof(Vertex),
	    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

	// Define the vertex input attribute.
	std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{
	    {{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, position)},
	     {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)}}};

	// Define the pipeline vertex input.
	VkPipelineVertexInputStateCreateInfo vertex_input{
	    .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	    .vertexBindingDescriptionCount   = 1,
	    .pVertexBindingDescriptions      = &binding_description,
	    .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
	    .pVertexAttributeDescriptions    = attribute_descriptions.data()};

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo raster{
	    .sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	    .cullMode  = VK_CULL_MODE_BACK_BIT,
	    .frontFace = VK_FRONT_FACE_CLOCKWISE,
	    .lineWidth = 1.0f};

	// Our attachment will write to all color channels, but no blending is enabled.
	VkPipelineColorBlendAttachmentState blend_attachment{
	    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

	VkPipelineColorBlendStateCreateInfo blend{
	    .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	    .attachmentCount = 1,
	    .pAttachments    = &blend_attachment};

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewport{
	    .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	    .viewportCount = 1,
	    .scissorCount  = 1};

	// Disable all depth testing.
	VkPipelineDepthStencilStateCreateInfo depth_stencil{
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisample{
	    .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	std::array<VkDynamicState, 2> dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamic{
	    .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	    .dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
	    .pDynamicStates    = dynamics.data()};

	// Load our SPIR-V shaders.

	// Samples support different shading languages, all of which are offline compiled to SPIR-V, the shader format that Vulkan uses.
	// The shading language to load for can be selected via command line
	std::string shader_folder{""};
	switch (get_shading_language())
	{
		case vkb::ShadingLanguage::HLSL:
			shader_folder = "hlsl";
			break;
		case vkb::ShadingLanguage::SLANG:
			shader_folder = "slang";
			break;
		default:
			shader_folder = "glsl";
	}

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};

	// Vertex stage of the pipeline
	shader_stages[0] = {
	    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	    .stage  = VK_SHADER_STAGE_VERTEX_BIT,
	    .module = load_shader_module("hello_triangle/" + shader_folder + "/triangle.vert.spv"),
	    .pName  = "main"};

	// Fragment stage of the pipeline
	shader_stages[1] = {
	    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
	    .module = load_shader_module("hello_triangle/" + shader_folder + "/triangle.frag.spv"),
	    .pName  = "main"};

	VkGraphicsPipelineCreateInfo pipe{
	    .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	    .stageCount          = static_cast<uint32_t>(shader_stages.size()),
	    .pStages             = shader_stages.data(),
	    .pVertexInputState   = &vertex_input,
	    .pInputAssemblyState = &input_assembly,
	    .pViewportState      = &viewport,
	    .pRasterizationState = &raster,
	    .pMultisampleState   = &multisample,
	    .pDepthStencilState  = &depth_stencil,
	    .pColorBlendState    = &blend,
	    .pDynamicState       = &dynamic,
	    .layout              = context.pipeline_layout,        // We need to specify the pipeline layout up front
	    .renderPass          = render_pass_data.front().render_pass        // We need to specify a render pass up front
	};

	VK_CHECK(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipe, nullptr, &context.pipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(context.device, shader_stages[0].module, nullptr);
	vkDestroyShaderModule(context.device, shader_stages[1].module, nullptr);
}

/**
 * @brief Acquires an image from the swapchain.
 * @param[out] image The swapchain index for the acquired image.
 * @returns Vulkan result code
 */
VkResult HelloManyTriangles::acquire_next_image(uint32_t *image)
{
	VkSemaphore acquire_semaphore;
	if (context.recycled_semaphores.empty())
	{
		VkSemaphoreCreateInfo info = {
		    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK(vkCreateSemaphore(context.device, &info, nullptr, &acquire_semaphore));
	}
	else
	{
		acquire_semaphore = context.recycled_semaphores.back();
		context.recycled_semaphores.pop_back();
	}

	VkResult res = vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, acquire_semaphore, VK_NULL_HANDLE, image);

	if (res != VK_SUCCESS)
	{
		context.recycled_semaphores.push_back(acquire_semaphore);
		return res;
	}

	// If we have outstanding fences for this swapchain image, wait for them to complete first.
	// After begin frame returns, it is safe to reuse or delete resources which
	// were used previously.
	//
	// We wait for fences which completes N frames earlier, so we do not stall,
	// waiting for all GPU work to complete before this returns.
	// Normally, this doesn't really block at all,
	// since we're waiting for old frames to have been completed, but just in case.
	if (context.per_frame[*image].queue_submit_fence != VK_NULL_HANDLE)
	{
		vkWaitForFences(context.device, 1, &context.per_frame[*image].queue_submit_fence, true, UINT64_MAX);
		vkResetFences(context.device, 1, &context.per_frame[*image].queue_submit_fence);
	}

	if (context.per_frame[*image].primary_command_pool != VK_NULL_HANDLE)
	{
		vkResetCommandPool(context.device, context.per_frame[*image].primary_command_pool, 0);
	}

	// Recycle the old semaphore back into the semaphore manager.
	VkSemaphore old_semaphore = context.per_frame[*image].swapchain_acquire_semaphore;

	if (old_semaphore != VK_NULL_HANDLE)
	{
		context.recycled_semaphores.push_back(old_semaphore);
	}

	context.per_frame[*image].swapchain_acquire_semaphore = acquire_semaphore;

	return VK_SUCCESS;
}

/**
 * @brief Gets a semaphore from the recycled list or creates a new one.
 * @returns A VkSemaphore handle.
 */
VkSemaphore HelloManyTriangles::get_semaphore()
{
	VkSemaphore semaphore;
	if (context.recycled_semaphores.empty())
	{
		VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK(vkCreateSemaphore(context.device, &info, nullptr, &semaphore));
	}
	else
	{
		semaphore = context.recycled_semaphores.back();
		context.recycled_semaphores.pop_back();
	}
	return semaphore;
}

/**
 * @brief Renders a triangle to the specified swapchain image.
 * @param swapchain_index The swapchain index for the image being rendered.
 */
void HelloManyTriangles::render_triangle(uint32_t swapchain_index, const std::vector<size_t> &render_passes, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore, VkFence fence)
{
	// Render to this framebuffer.
	VkFramebuffer framebuffer = context.swapchain_framebuffers[swapchain_index];

	// We are now allocating a command buffer per render_triangle call
	VkCommandBufferAllocateInfo cmd_buf_info{
	    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool        = context.per_frame[swapchain_index].primary_command_pool,
	    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1};
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(context.device, &cmd_buf_info, &cmd));

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo begin_info{
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
	// Begin command recording
	vkBeginCommandBuffer(cmd, &begin_info);

	// Bind the graphics pipeline once for all render passes
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline);

	// Set clear color values.
	VkClearValue clear_value{
	    .color = {{0.01f, 0.01f, 0.033f, 1.0f}}};

	float      scale              = 0.8f + 0.2f * sinf(accumulated_time);
//	float      scale              = 1.0;
	uint32_t   scaled_width       = static_cast<uint32_t>(context.swapchain_dimensions.width * scale);
	uint32_t   scaled_height      = static_cast<uint32_t>(context.swapchain_dimensions.height * scale);
	VkExtent2D render_area_extent = {scaled_width, scaled_height};
	VkOffset2D render_area_offset = {static_cast<int32_t>((context.swapchain_dimensions.width - scaled_width) / 2),
	                                 static_cast<int32_t>((context.swapchain_dimensions.height - scaled_height) / 2)};

	for (size_t i : render_passes)
	{	
		if (false) {
			// Insert a pipeline barrier to fully synchronize
			VkMemoryBarrier memoryBarrier = {.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			                                 .pNext         = nullptr,
			                                 .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
			                                 .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			};

			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, // srcStageMask
				VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, // dstStageMask
				0,                                  // dependencyFlags
				1, &memoryBarrier,                  // memoryBarrierCount, pMemoryBarriers
				0, nullptr,                         // bufferMemoryBarrierCount, pBufferMemoryBarriers
				0, nullptr                          // imageMemoryBarrierCount, pImageMemoryBarriers
			);	
		}

		const auto &pass_data = render_pass_data[i];

		if (debug_ext_enabled)
		{
			VkDebugUtilsLabelEXT label_info{};
			label_info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			label_info.pLabelName = pass_data.description.c_str();
			vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
		}
		VkDeviceSize offset = {0};
		vkCmdBindVertexBuffers(cmd, 0, 1, &pass_data.vertex_buffer, &offset);

		// Begin the render pass.
		VkRenderPassBeginInfo rp_begin{
		    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		    .renderPass      = pass_data.render_pass,
		    .framebuffer     = framebuffer,
		    .renderArea      = {.offset = render_area_offset, .extent = render_area_extent},
		    .clearValueCount = 1,
		    .pClearValues    = &clear_value};        // Clear value is only used by the first pass
		// We will add draw commands in the same command buffer.
		vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport vp{
		    .x        = static_cast<float>(render_area_offset.x),
		    .y        = static_cast<float>(render_area_offset.y),
		    .width    = static_cast<float>(scaled_width),
		    .height   = static_cast<float>(scaled_height),
		    .minDepth = 0.0f,
		    .maxDepth = 1.0f};
		// Set viewport dynamically
		vkCmdSetViewport(cmd, 0, 1, &vp);

		VkRect2D scissor{
		    .offset = render_area_offset,
		    .extent = render_area_extent};

		// Set scissor dynamically
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		// Draw the circle for this render pass
		vkCmdDraw(cmd, pass_data.vertex_count, 1, 0, 0);

		// Complete render pass.
		vkCmdEndRenderPass(cmd);
		if (debug_ext_enabled)
		{
			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
	}

	// Complete the command buffer.
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags wait_stage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo info{
	    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
	    .waitSemaphoreCount   = (wait_semaphore != VK_NULL_HANDLE) ? 1u : 0u,
	    .pWaitSemaphores      = &wait_semaphore,
	    .pWaitDstStageMask    = &wait_stage,
	    .commandBufferCount   = 1,
	    .pCommandBuffers      = &cmd,
	    .signalSemaphoreCount = (signal_semaphore != VK_NULL_HANDLE) ? 1u : 0u,
	    .pSignalSemaphores    = &signal_semaphore};
	// Submit command buffer to graphics queue
	ATrace_beginSection("queueJob");
	VK_CHECK(vkQueueSubmit(context.queue, 1, &info, fence));
	vkQueueWaitIdle(context.queue);
	ATrace_endSection();
	usleep(5000);
}

/**
 * @brief Presents an image to the swapchain.
 * @param index The swapchain index previously obtained from @ref acquire_next_image.
 * @returns Vulkan result code
 */
VkResult HelloManyTriangles::present_image(uint32_t index)
{
	VkPresentInfoKHR present{
	    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	    .waitSemaphoreCount = 1,
	    .pWaitSemaphores    = &context.per_frame[index].swapchain_release_semaphore,
	    .swapchainCount     = 1,
	    .pSwapchains        = &context.swapchain,
	    .pImageIndices      = &index,
	};
	// Present swapchain image
	return vkQueuePresentKHR(context.queue, &present);
}

/**
 * @brief Initializes the Vulkan framebuffers.
 */
void HelloManyTriangles::init_framebuffers()
{
	context.swapchain_framebuffers.clear();

	// Create framebuffer for each swapchain image view
	for (auto &image_view : context.swapchain_image_views)
	{
		// Build the framebuffer.
		VkFramebufferCreateInfo fb_info{
		    .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		    .renderPass      = render_pass_data.front().render_pass,
		    .attachmentCount = 1,
		    .pAttachments    = &image_view,
		    .width           = context.swapchain_dimensions.width,
		    .height          = context.swapchain_dimensions.height,
		    .layers          = 1};

		VkFramebuffer framebuffer;
		VK_CHECK(vkCreateFramebuffer(context.device, &fb_info, nullptr, &framebuffer));

		context.swapchain_framebuffers.push_back(framebuffer);
	}
}

HelloManyTriangles::HelloManyTriangles()
{
}

HelloManyTriangles::~HelloManyTriangles()
{
	// When destroying the application, we need to make sure the GPU is no longer accessing any resources
	// This is done by doing a device wait idle, which blocks until the GPU signals
	vkDeviceWaitIdle(context.device);

	for (auto &framebuffer : context.swapchain_framebuffers)
	{
		vkDestroyFramebuffer(context.device, framebuffer, nullptr);
	}

	for (auto &per_frame : context.per_frame)
	{
		teardown_per_frame(per_frame);
	}

	context.per_frame.clear();

	for (auto semaphore : context.recycled_semaphores)
	{
		vkDestroySemaphore(context.device, semaphore, nullptr);
	}

	if (context.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(context.device, context.pipeline, nullptr);
	}

	if (context.pipeline_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(context.device, context.pipeline_layout, nullptr);
	}

	for (auto &pass_data : render_pass_data)
	{
		vkDestroyRenderPass(context.device, pass_data.render_pass, nullptr);
		vmaDestroyBuffer(context.vma_allocator, pass_data.vertex_buffer, pass_data.vertex_buffer_allocation);
	}
	
	for (VkImageView image_view : context.swapchain_image_views)
	{
		vkDestroyImageView(context.device, image_view, nullptr);
	}

	if (context.swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
	}

	if (context.surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
	}

	if (context.vma_allocator != VK_NULL_HANDLE)
	{
		vmaDestroyAllocator(context.vma_allocator);
	}

	if (context.device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(context.device, nullptr);
	}

	vk_instance.reset();
}

void HelloManyTriangles::init_render_passes() {
	add_render_pass("RP 1: scale 2, 150k triangles", {0.0f, 0.0f}, 2.0f, 150000, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 2: scale 0.3, 100k triangles", {0.5f, 0.0f}, 0.3f, 100000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 3: out of screen, 100k triangles", {2.5f, 0.0f}, 0.3f, 100000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 4: scale 0.1, 200k triangles", {0.0f, 0.5f}, 0.1f, 200000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

bool HelloManyTriangles::prepare(const vkb::ApplicationOptions &options)
{
	// Headless is not supported to keep this sample as simple as possible
	assert(options.window != nullptr);
	assert(options.window->get_window_mode() != vkb::Window::Mode::Headless);

	init_instance();

	vk_instance = std::make_unique<vkb::Instance>(context.instance);

	context.surface                     = options.window->create_surface(*vk_instance);
	auto &extent                        = options.window->get_extent();
	context.swapchain_dimensions.width  = extent.width;
	context.swapchain_dimensions.height = extent.height;

	if (!context.surface)
	{
		throw std::runtime_error("Failed to create window surface.");
	}

	init_device();

	init_swapchain();

	// Create the necessary objects for rendering.
    init_render_passes();
	init_pipeline();
	init_framebuffers();

	return true;
}

void HelloManyTriangles::update(float delta_time)
{
	accumulated_time += delta_time;

	uint32_t index;

	auto res = acquire_next_image(&index);

	// Handle outdated error in acquire.
	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resize(context.swapchain_dimensions.width, context.swapchain_dimensions.height);
		res = acquire_next_image(&index);
	}

	if (res != VK_SUCCESS)
	{
		vkQueueWaitIdle(context.queue);
		return;
	}

	if (context.per_frame[index].swapchain_release_semaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo semaphore_info{
		    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &context.per_frame[index].swapchain_release_semaphore));
	}

#if 0
	render_triangle(index, {0, 1, 2, 3}, context.per_frame[index].swapchain_acquire_semaphore, context.per_frame[index].swapchain_release_semaphore, context.per_frame[index].queue_submit_fence);
#else
	VkSemaphore render_semaphore1 = get_semaphore();
	VkSemaphore render_semaphore2 = get_semaphore();
	VkSemaphore render_semaphore3 = get_semaphore();
	render_triangle(index, {0}, context.per_frame[index].swapchain_acquire_semaphore, render_semaphore1, VK_NULL_HANDLE);
	render_triangle(index, {1}, render_semaphore1, render_semaphore2, VK_NULL_HANDLE);
	render_triangle(index, {2}, render_semaphore2, render_semaphore3, VK_NULL_HANDLE);
	render_triangle(index, {3}, render_semaphore3, context.per_frame[index].swapchain_release_semaphore, context.per_frame[index].queue_submit_fence);
	context.recycled_semaphores.push_back(render_semaphore1);
	context.recycled_semaphores.push_back(render_semaphore2);
	context.recycled_semaphores.push_back(render_semaphore3);
#endif
	res = present_image(index);

	// Handle Outdated error in present.
	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR)
	{
		resize(context.swapchain_dimensions.width, context.swapchain_dimensions.height);
	}
	else if (res != VK_SUCCESS)
	{
		LOGE("Failed to present swapchain image.");
	}
}

bool HelloManyTriangles::resize(const uint32_t, const uint32_t)
{
	if (context.device == VK_NULL_HANDLE)
	{
		return false;
	}

	VkSurfaceCapabilitiesKHR surface_properties;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu, context.surface, &surface_properties));

	// Only rebuild the swapchain if the dimensions have changed
	if (surface_properties.currentExtent.width == context.swapchain_dimensions.width &&
	    surface_properties.currentExtent.height == context.swapchain_dimensions.height)
	{
		return false;
	}

	vkDeviceWaitIdle(context.device);

	for (auto &framebuffer : context.swapchain_framebuffers)
	{
		vkDestroyFramebuffer(context.device, framebuffer, nullptr);
	}

	init_swapchain();
	init_framebuffers();
	return true;
}

std::unique_ptr<vkb::Application> create_hello_many_triangles()
{
	return std::make_unique<HelloManyTriangles>();
}
