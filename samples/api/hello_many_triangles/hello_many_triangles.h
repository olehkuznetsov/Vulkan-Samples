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

#include "api_vulkan_sample.h"

class HelloManyTriangles : public ApiVulkanSample
{
  public:
	HelloManyTriangles();

	~HelloManyTriangles() override;

	void render(float delta_time) override;

	void build_command_buffers() override;

	bool prepare(const vkb::ApplicationOptions &options) override;

  private:
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

	VkPipeline       pipeline        = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

	void render_triangle(uint32_t swapchain_index, const std::vector<size_t> &render_passes, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore, VkFence fence);

	void add_render_pass(const char *description, const glm::vec2 &offset, float scale, uint32_t triangle_count, VkAttachmentLoadOp load_op, VkImageLayout initial_layout, VkImageLayout final_layout);
	void prepare_render_passes();
	void prepare_pipelines();
	bool debug_ext_enabled = false;
};

std::unique_ptr<vkb::Application> create_hello_many_triangles();
