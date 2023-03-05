#include "engine/graphics/Common.hpp"
#include <engine/graphics/renderer/AerialPerspectiveRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include <set>

#include <scattering.h>
#include <ap_renderer.h>

namespace en
{
	AerialPerspectiveRenderer::AerialPerspectiveRenderer(
		size_t max_concurrent,
		uint32_t width,
		uint32_t height,
		const Camera* camera,
		const Sun &sun,
		vk::Swapchain &swapchain,
		Atmosphere &atmosphere,
		Precomputer &precomp,
		AerialPerspective &aerial ) :

		m_FrameWidth(width),
		m_FrameHeight(height),
		m_Camera(camera),
		m_Sun(sun),
		m_VertShader("sky/aerial_perspective.vert", false),
		m_FragShader("sky/aerial_perspective.frag", false),
		m_GraphicsCommandPool(0, VulkanAPI::GetGraphicsQFI()),
		m_Atmosphere(atmosphere),
		m_Precomp(precomp),
		m_Aerial{aerial},
		m_MaxConcurrent{max_concurrent},
		m_GraphicsPipeline{VK_NULL_HANDLE},
		m_ColorInputAttachmentDSs(max_concurrent, VK_NULL_HANDLE),
		m_DepthInputAttachmentDSs(max_concurrent, VK_NULL_HANDLE),
		m_DescriptorPool{VK_NULL_HANDLE}
	{
		VkDevice device = VulkanAPI::GetDevice();

		CreateCommandBuffers();
		CreatePipelineLayout(device);
	}

	void AerialPerspectiveRenderer::Render(VkQueue queue, size_t imageIndx) const
	{
		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_GraphicsCommandBuffers[imageIndx];
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		VkResult result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		ASSERT_VULKAN(result);
	}

	void AerialPerspectiveRenderer::Destroy()
	{
		VkDevice device = VulkanAPI::GetDevice();

		m_GraphicsCommandPool.Destroy();

		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_InputAttachmentDSL, nullptr);

		m_VertShader.Destroy();
		m_FragShader.Destroy();
	}

	void AerialPerspectiveRenderer::ResizeFrame(uint32_t width, uint32_t height)
	{
		m_FrameWidth = width;
		m_FrameHeight = height;
	}

	void AerialPerspectiveRenderer::CreatePipelineLayout(VkDevice device)
	{
		VkDescriptorSetLayoutBinding inputAttachmentBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutCreateInfo dsLayoutCreateInfo;
		dsLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		dsLayoutCreateInfo.pNext = nullptr;
		dsLayoutCreateInfo.bindingCount = 1;
		dsLayoutCreateInfo.pBindings = &inputAttachmentBinding;
		dsLayoutCreateInfo.flags = 0;
		ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &dsLayoutCreateInfo, nullptr, &m_InputAttachmentDSL));

		std::vector<VkDescriptorSetLayout> descSetLayouts;
		descSetLayouts.resize(8);
		descSetLayouts[APR_SETS_CAM] = Camera::GetDescriptorSetLayout();
		descSetLayouts[APR_SETS_SUN] = m_Sun.GetDescriptorSetLayout();
		descSetLayouts[APR_SETS_RATIO] = m_Precomp.GetRatioDescriptorSetLayout();
		descSetLayouts[APR_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSetLayout();
		descSetLayouts[APR_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSetLayout();
		descSetLayouts[APR_SETS_DEPTH_INPUT_ATTACHMENT] = m_InputAttachmentDSL;
		descSetLayouts[APR_SETS_COLOR_INPUT_ATTACHMENT] = m_InputAttachmentDSL;
		descSetLayouts[APR_SETS_AERIAL_PERSPECTIVE] = m_Aerial.GetSampleDescriptorLayout();

		VkPipelineLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.setLayoutCount = descSetLayouts.size();
		layoutCreateInfo.pSetLayouts = descSetLayouts.data();
		layoutCreateInfo.pushConstantRangeCount = 0;
		layoutCreateInfo.pPushConstantRanges = nullptr;

		VkResult result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &m_PipelineLayout);
		ASSERT_VULKAN(result);
	}

	std::pair<VkSubpassDescription, VkSubpassContents> AerialPerspectiveRenderer::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &color_attachment,
		const VkAttachmentReference &depth_attachment,
		const VkAttachmentReference &swapchain_attachment) {

		m_InputAttachmentRefs[0] = color_attachment;
		// need to read from and write to attachment, general for now.
		m_InputAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_GENERAL;
		m_InputAttachmentRefs[1] = depth_attachment;
		m_InputAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_GENERAL;

		// no need to preserve any attachment, this is the last stage.
		return {
			VkSubpassDescription {
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = static_cast<uint32_t>(m_InputAttachmentRefs.size()),
				.pInputAttachments = m_InputAttachmentRefs.data(),
				.colorAttachmentCount = 1,
				.pColorAttachments = &m_InputAttachmentRefs[0],
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = &m_InputAttachmentRefs[1],
				.preserveAttachmentCount = 0,
				// should be preserved for later operations.
				.pPreserveAttachments = nullptr,
			},
			VK_SUBPASS_CONTENTS_INLINE
		};
	}

	void AerialPerspectiveRenderer::AllocateResources(
		std::vector<VkImageView> &colorImageViews,
		std::vector<VkImageView> &depthImageViews,
		std::vector<VkImageView> &swapchainImageViews,
		std::vector<VkFramebuffer> &framebuffers,
		VkRenderPass renderpass) {

		// destroy resources.
		VkDevice device = VulkanAPI::GetDevice();
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);

		// one descriptor per color- and depth-image.
		const uint32_t imageCount = colorImageViews.size();

		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = imageCount*2
		};

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = imageCount*2;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		ASSERT_VULKAN(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));

		std::vector<VkDescriptorSetLayout> allocLayouts(imageCount, m_InputAttachmentDSL);

		VkDescriptorSetAllocateInfo allocInfo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
			.pSetLayouts = allocLayouts.data()
		};

		m_ColorInputAttachmentDSs.resize(imageCount);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, m_ColorInputAttachmentDSs.data()));
		m_DepthInputAttachmentDSs.resize(imageCount);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, m_DepthInputAttachmentDSs.data()));

		std::vector<VkDescriptorImageInfo> texInfo(imageCount*2, {
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL
		});
		std::vector<VkWriteDescriptorSet> writeDescSets(imageCount*2, {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		});

		for (int i = 0; i != imageCount; ++i) {
			texInfo[i].imageView = colorImageViews[i];
			writeDescSets[i].pImageInfo = &texInfo[i];

			writeDescSets[i].dstSet = m_ColorInputAttachmentDSs[i];

			texInfo[imageCount + i].imageView = depthImageViews[i];
			writeDescSets[imageCount + i].pImageInfo = &texInfo[imageCount + i];

			writeDescSets[imageCount + i].dstSet = m_DepthInputAttachmentDSs[i];
		}
		vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
	}

	void AerialPerspectiveRenderer::CreatePipeline(size_t subpass, VkRenderPass renderPass)
	{
		VkDevice device = VulkanAPI::GetDevice();
		vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);

		// Shader stage
		VkPipelineShaderStageCreateInfo vertStageCreateInfo;
		vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageCreateInfo.pNext = nullptr;
		vertStageCreateInfo.flags = 0;
		vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageCreateInfo.module = m_VertShader.GetVulkanModule();
		vertStageCreateInfo.pName = "main";
		vertStageCreateInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragStageCreateInfo;
		fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageCreateInfo.pNext = nullptr;
		fragStageCreateInfo.flags = 0;
		fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageCreateInfo.module = m_FragShader.GetVulkanModule();
		fragStageCreateInfo.pName = "main";
		fragStageCreateInfo.pSpecializationInfo = nullptr;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertStageCreateInfo, fragStageCreateInfo };

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo;
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.pNext = nullptr;
		vertexInputCreateInfo.flags = 0;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

		// Input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.pNext = nullptr;
		inputAssemblyCreateInfo.flags = 0;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// Viewports and scissors
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(m_FrameHeight);
		viewport.width = static_cast<float>(m_FrameWidth);
		viewport.height = -static_cast<float>(m_FrameHeight);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_FrameWidth, m_FrameHeight };

		VkPipelineViewportStateCreateInfo viewportCreateInfo;
		viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCreateInfo.pNext = nullptr;
		viewportCreateInfo.flags = 0;
		viewportCreateInfo.viewportCount = 1;
		viewportCreateInfo.pViewports = &viewport;
		viewportCreateInfo.scissorCount = 1;
		viewportCreateInfo.pScissors = &scissor;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo;
		rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerCreateInfo.pNext = nullptr;
		rasterizerCreateInfo.flags = 0;
		rasterizerCreateInfo.depthClampEnable = VK_FALSE;
		rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizerCreateInfo.depthBiasClamp = 0.0f;
		rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;
		rasterizerCreateInfo.lineWidth = 1.0f;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.pNext = nullptr;
		multisampleCreateInfo.flags = 0;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.minSampleShading = 1.0f;
		multisampleCreateInfo.pSampleMask = nullptr;
		multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

		// Depth and stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.pNext = nullptr;
		depthStencilCreateInfo.flags = 0;
		depthStencilCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
		// aerial perspective should only be applied to geometry, not atmosphere (rendered to quad over back plane).
		// -> also render aerial perspective to quad on back plane, test with GREATER.
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilCreateInfo.front = {};
		depthStencilCreateInfo.back = {};
		depthStencilCreateInfo.minDepthBounds = 0.0f;
		depthStencilCreateInfo.maxDepthBounds = 1.0f;

		// Color blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo;
		colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCreateInfo.pNext = nullptr;
		colorBlendCreateInfo.flags = 0;
		colorBlendCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendCreateInfo.attachmentCount = 1;
		colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
		colorBlendCreateInfo.blendConstants[0] = 0.0f;
		colorBlendCreateInfo.blendConstants[1] = 0.0f;
		colorBlendCreateInfo.blendConstants[2] = 0.0f;
		colorBlendCreateInfo.blendConstants[3] = 0.0f;

		// Dynamic states
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.pNext = nullptr;
		dynamicStateCreateInfo.flags = 0;
		dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

		// Creation
		VkGraphicsPipelineCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.stageCount = shaderStages.size();
		createInfo.pStages = shaderStages.data();
		createInfo.pVertexInputState = &vertexInputCreateInfo;
		createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		createInfo.pTessellationState = nullptr;
		createInfo.pViewportState = &viewportCreateInfo;
		createInfo.pRasterizationState = &rasterizerCreateInfo;
		createInfo.pMultisampleState = &multisampleCreateInfo;
		createInfo.pDepthStencilState = &depthStencilCreateInfo;
		createInfo.pColorBlendState = &colorBlendCreateInfo;
		createInfo.pDynamicState = &dynamicStateCreateInfo;
		createInfo.layout = m_PipelineLayout;
		createInfo.renderPass = renderPass;
		createInfo.subpass = subpass;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_GraphicsPipeline);
		ASSERT_VULKAN(result);
	}

	void AerialPerspectiveRenderer::CreateCommandBuffers()
	{
		// one buffer per swapchain-image.
		m_GraphicsCommandPool.AllocateBuffers(m_MaxConcurrent, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		m_GraphicsCommandBuffers = m_GraphicsCommandPool.GetBuffers();
	}

	void AerialPerspectiveRenderer::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx)
	{
		// Bind pipeline
		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
		std::vector<VkDescriptorSet> sets;
		sets.resize(8);
		sets[APR_SETS_CAM] = m_Camera->GetDescriptorSet();
		sets[APR_SETS_SUN] = m_Sun.GetDescriptorSet();
		sets[APR_SETS_RATIO] = m_Precomp.GetRatioDescriptorSet();
		sets[APR_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSet();
		sets[APR_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSet();
		sets[APR_SETS_DEPTH_INPUT_ATTACHMENT] = m_DepthInputAttachmentDSs[frame_indx];
		// sample from imageView for current color attachment.
		sets[APR_SETS_COLOR_INPUT_ATTACHMENT] = m_ColorInputAttachmentDSs[frame_indx];
		sets[APR_SETS_AERIAL_PERSPECTIVE] = m_Aerial.GetSampleDescriptor();

		vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

		// Viewport
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(m_FrameHeight);
		viewport.width = static_cast<float>(m_FrameWidth);
		viewport.height = -static_cast<float>(m_FrameHeight);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(buf, 0, 1, &viewport);

		// Scissor
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_FrameWidth, m_FrameHeight };

		vkCmdSetScissor(buf, 0, 1, &scissor);

		vkCmdDraw(buf, 6, 1, 0, 0);
	}
}
