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

#include "hello_many_triangles.h"

#include <common/vk_common.h>
#include <core/util/logging.hpp>
#include "core/allocated.h"

HelloManyTriangles::HelloManyTriangles()
{
	title = "Hello many triangles";
}

HelloManyTriangles::~HelloManyTriangles()
{
	if (has_device())
	{
		vkDeviceWaitIdle(get_device().get_handle());
		vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
		for (auto &pass_data : render_pass_data)
		{
			vkDestroyRenderPass(get_device().get_handle(), pass_data.render_pass, nullptr);
			vmaDestroyBuffer(vkb::allocated::get_memory_allocator(), pass_data.vertex_buffer, pass_data.vertex_buffer_allocation);
		}
	}
}

void HelloManyTriangles::add_render_pass(const char *description, const glm::vec2 &offset, float scale, uint32_t triangle_count, VkAttachmentLoadOp load_op, VkImageLayout initial_layout, VkImageLayout final_layout)
{
	RenderPassData data;

	data.description = description;

	// Generate circle vertices
	std::vector<Vertex> vertices;
	vertices.reserve(triangle_count * 3);
	int start_seed = rand() % triangle_count;
	for (uint32_t i = 0; i < triangle_count; ++i)
	{
		float angle1 = 2.0f * glm::pi<float>() * static_cast<float>(i) / triangle_count;
		float angle2 = 2.0f * glm::pi<float>() * static_cast<float>(i + 1) / triangle_count;

		float k1 = 0.9f + sin(10.f * 2.0f * glm::pi<float>() * (i + start_seed) / triangle_count) / 10.0f;
		float k2 = 0.9f + sin(10.f * 2.0f * glm::pi<float>() * (i + 1 + start_seed) / triangle_count) / 10.0f;

		// Center vertex
		vertices.push_back({{offset.x, offset.y, 0.5f}, {1.0f, 1.0f, 1.0f}});

		// First vertex on circumference
		vertices.push_back({{offset.x + cos(angle1) * scale * k1, offset.y + sin(angle1) * scale * k1, 0.5f}, {0.5f + 0.5f * cos(angle1), 0.5f + 0.5f * sin(angle1), 0.0f}});

		// Second vertex on circumference
		vertices.push_back({{offset.x + cos(angle2) * scale * k2, offset.y + sin(angle2) * scale * k2, 0.5f}, {0.5f + 0.5f * cos(angle2), 0.5f + 0.5f * sin(angle2), 0.0f}});
	}
	data.vertex_count = vertices.size();

	// Create vertex buffer
	const VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
	VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = buffer_size, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
	VmaAllocationCreateInfo buffer_alloc_ci{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
	VmaAllocationInfo       buffer_alloc_info{};
	vmaCreateBuffer(vkb::allocated::get_memory_allocator(), &buffer_info, &buffer_alloc_ci, &data.vertex_buffer, &data.vertex_buffer_allocation, &buffer_alloc_info);
	get_device().get_debug_utils().set_debug_name(get_device().get_handle(), VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(data.vertex_buffer), (std::string(description) + " - Vertex Buffer").c_str());
	if (buffer_alloc_info.pMappedData)
	{
		memcpy(buffer_alloc_info.pMappedData, vertices.data(), buffer_size);
	}
	else
	{
		throw std::runtime_error("Could not map vertex buffer.");
	}

	// Create render pass
	std::array<VkAttachmentDescription, 2> attachments{};
	// Color attachment
	attachments[0] = {
	    .format         = get_render_context().get_format(),
	    .samples        = VK_SAMPLE_COUNT_1_BIT,
	    .loadOp         = load_op,
	    .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
	    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    .initialLayout  = initial_layout,
	    .finalLayout    = final_layout};

	// Depth attachment
	attachments[1] = {
	    .format         = depth_format,
	    .samples        = VK_SAMPLE_COUNT_1_BIT,
	    .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
	    .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
	    .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depth_ref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
	                             .colorAttachmentCount    = 1,
	                             .pColorAttachments       = &color_ref,
	                             .pDepthStencilAttachment = &depth_ref};

	std::array<VkSubpassDependency, 2> dependencies{};

	dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass      = 0;
	dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask   = VK_ACCESS_NONE_KHR;
	dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass      = 0;
	dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo rp_info{.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
	                               .attachmentCount = static_cast<uint32_t>(attachments.size()),
	                               .pAttachments    = attachments.data(),
	                               .subpassCount    = 1,
	                               .pSubpasses      = &subpass,
	                               .dependencyCount = static_cast<uint32_t>(dependencies.size()),
	                               .pDependencies   = dependencies.data()};

	VK_CHECK(vkCreateRenderPass(get_device().get_handle(), &rp_info, nullptr, &data.render_pass));
	get_device().get_debug_utils().set_debug_name(get_device().get_handle(), VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<uint64_t>(data.render_pass), description);

	render_pass_data.push_back(data);
}

void HelloManyTriangles::prepare_pipelines()
{
	// Create a blank pipeline layout.
	// We are not binding any resources to the pipeline in this first sample.
	VkPipelineLayoutCreateInfo layout_info{
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &layout_info, nullptr, &pipeline_layout));
	get_device().get_debug_utils().set_debug_name(get_device().get_handle(), VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(pipeline_layout), "HelloManyTrianglesPipelineLayout");

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
	std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{{{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
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

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};

	// Vertex stage of the pipeline
	shader_stages[0] = load_shader("hello_triangle/hlsl/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);

	// Fragment stage of the pipeline
	shader_stages[1] = load_shader("hello_triangle/hlsl/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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
	    .layout              = pipeline_layout,        // We need to specify the pipeline layout up front
	    .renderPass          = render_pass_data.front().render_pass        // We need to specify a render pass up front
	};

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipe, nullptr, &pipeline));
	get_device().get_debug_utils().set_debug_name(get_device().get_handle(), VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(pipeline), "HelloManyTrianglesPipeline");
}

void HelloManyTriangles::render(float delta_time)
{
	ApiVulkanSample::prepare_frame();
	render_triangle(current_buffer, {0, 1, 2, 3}, semaphores.acquired_image_ready, semaphores.render_complete, VK_NULL_HANDLE);
	ApiVulkanSample::submit_frame();
}

void HelloManyTriangles::prepare_render_passes()
{
	add_render_pass("RP 1: scale 2, 150k triangles", {0.0f, 0.0f}, 2.0f, 150000, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 2: scale 0.3, 100k triangles", {0.5f, 0.0f}, 0.3f, 100000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 3: out of screen, 100k triangles", {2.5f, 0.0f}, 0.3f, 100000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	add_render_pass("RP 4: scale 0.1, 200k triangles", {0.0f, 0.5f}, 0.1f, 200000, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

bool HelloManyTriangles::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	// Create the necessary objects for rendering.
	prepare_render_passes();
	prepare_pipelines();
	build_command_buffers();

	prepared = true;
	return true;
}

void HelloManyTriangles::build_command_buffers()
{
}

std::unique_ptr<vkb::Application> create_hello_many_triangles()
{
	return std::make_unique<HelloManyTriangles>();
}

/**
 * @brief Renders a triangle to the specified swapchain image.
 * @param swapchain_index The swapchain index for the image being rendered.
 */
void HelloManyTriangles::render_triangle(uint32_t swapchain_index, const std::vector<size_t> &render_passes, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore, VkFence fence)
{
	VkDevice device = get_device().get_handle();
	// Render to this framebuffer.
	VkFramebuffer framebuffer = framebuffers[swapchain_index];

	// We are now allocating a command buffer per render_triangle call
	VkCommandBufferAllocateInfo cmd_buf_info{
	    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool        = cmd_pool,
	    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1};
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(device, &cmd_buf_info, &cmd));

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo begin_info{
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
	// Begin command recording
	vkBeginCommandBuffer(cmd, &begin_info);

	// Bind the graphics pipeline once for all render passes
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// Set clear color values.
	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color        = {{0.01f, 0.01f, 0.033f, 1.0f}};
	clear_values[1].depthStencil = {1.0f, 0};

	float      scale              = 1.0;
	uint32_t   scaled_width       = static_cast<uint32_t>(get_render_context().get_surface_extent().width * scale);
	uint32_t   scaled_height      = static_cast<uint32_t>(get_render_context().get_surface_extent().height * scale);
	VkExtent2D render_area_extent = {scaled_width, scaled_height};
	VkOffset2D render_area_offset = {static_cast<int32_t>((get_render_context().get_surface_extent().width - scaled_width) / 2),
	                                 static_cast<int32_t>((get_render_context().get_surface_extent().height - scaled_height) / 2)};

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
		    .clearValueCount = static_cast<uint32_t>(clear_values.size()),
		    .pClearValues    = clear_values.data()};
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
	VK_CHECK(vkQueueSubmit(queue, 1, &info, fence));
}
