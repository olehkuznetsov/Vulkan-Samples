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

#pragma once

#include "common/vk_common.h"
#include <string>
#include "core/instance.h"
#include "platform/application.h"

/**
 * @brief A self-contained (minimal use of framework) sample that illustrates
 * the rendering of a triangle
 */
class HelloManyTriangles : public vkb::Application
{
	/**
	 * @brief Swapchain state
	 */
	struct SwapchainDimensions
	{
		/// Width of the swapchain.
		uint32_t width = 0;

		/// Height of the swapchain.
		uint32_t height = 0;

		/// Pixel format of the swapchain.
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	/**
	 * @brief Per-frame data
	 */
	struct PerFrame
	{
		VkFence         queue_submit_fence          = VK_NULL_HANDLE;
		VkCommandPool   primary_command_pool        = VK_NULL_HANDLE;
		VkSemaphore     swapchain_acquire_semaphore = VK_NULL_HANDLE;
		VkSemaphore     swapchain_release_semaphore = VK_NULL_HANDLE;
	};

	/**
	 * @brief Vulkan objects and global state
	 */
	struct Context
	{
		/// The Vulkan instance.
		VkInstance instance = VK_NULL_HANDLE;

		/// The Vulkan physical device.
		VkPhysicalDevice gpu = VK_NULL_HANDLE;

		/// The Vulkan device.
		VkDevice device = VK_NULL_HANDLE;

		/// The Vulkan device queue.
		VkQueue queue = VK_NULL_HANDLE;

		/// The swapchain.
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;

		/// The swapchain dimensions.
		SwapchainDimensions swapchain_dimensions;

		/// The surface we will render to.
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		/// The queue family index where graphics work will be submitted.
		int32_t graphics_queue_index = -1;

		/// The image view for each swapchain image.
		std::vector<VkImageView> swapchain_image_views;

		/// The framebuffers, one for each swapchain image view.
		/// The render passes are compatible so we only need one set of framebuffers.
		std::vector<VkFramebuffer> swapchain_framebuffers;
		/// The graphics pipeline.
		VkPipeline pipeline = VK_NULL_HANDLE;

		/**
		 * The pipeline layout for resources.
		 * Not used in this sample, but we still need to provide a dummy one.
		 */
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

		/// A set of semaphores that can be reused.
		std::vector<VkSemaphore> recycled_semaphores;

		/// A set of per-frame data.
		std::vector<PerFrame> per_frame;

		VmaAllocator vma_allocator = VK_NULL_HANDLE;
	};

	struct RenderPassData
	{
		std::string   description;
		VkRenderPass  render_pass              = VK_NULL_HANDLE;
		VkBuffer      vertex_buffer            = VK_NULL_HANDLE;
		VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;
		uint32_t      vertex_count             = 0;
	};

	/// Vulkan objects for each render pass
	std::vector<RenderPassData> render_pass_data;

	/// Properties of the vertices used in this sample.
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 color;
	};

  public:
	HelloManyTriangles();

	virtual ~HelloManyTriangles();

	virtual bool prepare(const vkb::ApplicationOptions &options) override;

	virtual void update(float delta_time) override;

	virtual bool resize(const uint32_t width, const uint32_t height) override;

  private:
	bool validate_extensions(const std::vector<const char *>          &required,
	                         const std::vector<VkExtensionProperties> &available);

	void init_instance();

	void init_device();
	void init_per_frame(PerFrame &per_frame);
	void teardown_per_frame(PerFrame &per_frame);

	void init_swapchain();

	void add_render_pass(const char* description, const glm::vec2 &offset, float scale, uint32_t triangle_count, VkAttachmentLoadOp load_op, VkImageLayout final_layout);

	VkShaderModule load_shader_module(const std::string &path);

	void init_pipeline();

	VkResult acquire_next_image(uint32_t *image);

	VkSemaphore get_semaphore();

	void render_triangle(uint32_t swapchain_index, const std::vector<size_t> &render_passes, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore, VkFence fence);

	VkResult present_image(uint32_t index);

	void init_framebuffers();

	void init_render_passes();

  private:
	Context context;

	std::unique_ptr<vkb::Instance> vk_instance;

	float accumulated_time = 0.0f;

	bool debug_ext_enabled = false;
};

std::unique_ptr<vkb::Application> create_hello_many_triangles();
