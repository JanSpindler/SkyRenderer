//#include <alloca.h>
#include <engine/graphics/renderer/PostprocessingSubpass.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace en
{
	PostprocessingSubpass::PostprocessingSubpass(uint32_t width, uint32_t height) :
		m_VertShader("post_process/back_quad.vert", false),
		m_FragShader("post_process/back_quad.frag", false),
		m_Width{width},
		m_Height{height},
		m_DescriptorPool{VK_NULL_HANDLE},
		m_Pipeline{VK_NULL_HANDLE}
	{
		VkDevice device = VulkanAPI::GetDevice();

		CreateLayouts(device);
	}

	PostprocessingSubpass::~PostprocessingSubpass() {
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_InputAttachmentDSLayout, nullptr);

		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_Pipeline, nullptr);

		m_VertShader.Destroy();
		m_FragShader.Destroy();
	}

	void PostprocessingSubpass::Resize(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;
	}

	std::pair<VkSubpassDescription, VkSubpassContents> PostprocessingSubpass::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &color_attachment,
		const VkAttachmentReference &depth_attachment,
		const VkAttachmentReference &swapchain_attachment) {

		m_InputAttachmentRef = color_attachment;
		m_InputAttachmentRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// no need to preserve any attachment, this is the last stage.
		return {
			VkSubpassDescription {
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 1,
				.pInputAttachments = &m_InputAttachmentRef,
				.colorAttachmentCount = 1,
				.pColorAttachments = &swapchain_attachment,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = 0,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr,
			},
			VK_SUBPASS_CONTENTS_INLINE
		};
	}

	void PostprocessingSubpass::AllocateResources(
		std::vector<VkImageView> &colorImageViews,
		std::vector<VkImageView> &depthImageViews,
		std::vector<VkImageView> &swapchainImageViews,
		std::vector<VkFramebuffer> &framebuffers,
		VkRenderPass renderpass) {

		// destroy resources.
		VkDevice device = VulkanAPI::GetDevice();
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);

		const uint32_t descriptorCount = colorImageViews.size();

		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			// one descriptor per image.
			.descriptorCount = descriptorCount
		};

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = descriptorCount;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		ASSERT_VULKAN(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));

		std::vector<VkDescriptorSetLayout> allocLayouts(descriptorCount);
		std::fill_n(allocLayouts.begin(), descriptorCount, m_InputAttachmentDSLayout);

		VkDescriptorSetAllocateInfo allocInfo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
			.pSetLayouts = allocLayouts.data()
		};

		m_InputAttachmentDSs.resize(descriptorCount);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, m_InputAttachmentDSs.data()));

		std::vector<VkDescriptorImageInfo> texInfo(descriptorCount, {
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
		std::vector<VkWriteDescriptorSet> writeDescSets(descriptorCount, {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		});

		for (int i = 0; i != descriptorCount; ++i) {
			texInfo[i].imageView = colorImageViews[i];
			writeDescSets[i].pImageInfo = &texInfo[i];

			writeDescSets[i].dstSet = m_InputAttachmentDSs[i];
		}
		vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
	}

	void PostprocessingSubpass::CreateLayouts(VkDevice device)
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
		ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &dsLayoutCreateInfo, nullptr, &m_InputAttachmentDSLayout));

		VkPipelineLayoutCreateInfo pLayoutCreateInfo;
		pLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pLayoutCreateInfo.pNext = nullptr;
		pLayoutCreateInfo.flags = 0;
		pLayoutCreateInfo.setLayoutCount = 1;
		pLayoutCreateInfo.pSetLayouts = &m_InputAttachmentDSLayout;
		pLayoutCreateInfo.pushConstantRangeCount = 0;
		pLayoutCreateInfo.pPushConstantRanges = nullptr;

		VkResult result = vkCreatePipelineLayout(device, &pLayoutCreateInfo, nullptr, &m_PipelineLayout);
		ASSERT_VULKAN(result);
	}

	void PostprocessingSubpass::CreatePipeline(size_t subpass, VkRenderPass renderPass)
	{
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroyPipeline(device, m_Pipeline, nullptr);
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

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(m_Height);
		viewport.width = static_cast<float>(m_Width);
		viewport.height = -static_cast<float>(m_Height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Width, m_Height };

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
		// post-processing applies to all pixels.
		depthStencilCreateInfo.depthTestEnable = VK_FALSE;
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
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
		// render pass from subpassRenderer.
		createInfo.renderPass = renderPass;
		createInfo.subpass = subpass;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_Pipeline);
		ASSERT_VULKAN(result);
	}

	void PostprocessingSubpass::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx)
	{
		// May be better to record into a secondary command buffer, since this
		// doesn't change between frames, but this is basically nothing.

		// Bind pipeline
		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

		// Viewport
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(m_Height);
		viewport.width = static_cast<float>(m_Width);
		viewport.height = -static_cast<float>(m_Height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(buf, 0, 1, &viewport);

		// Scissor
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Width, m_Height };

		vkCmdSetScissor(buf, 0, 1, &scissor);

		vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_InputAttachmentDSs[frame_indx], 0, nullptr);
		// draw fullscreen-quad (two triangles).
		vkCmdDraw(buf, 6, 1, 0, 0);
	}
}
