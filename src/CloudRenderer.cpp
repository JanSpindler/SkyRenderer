#include <engine/graphics/renderer/CloudRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace en
{
	CloudRenderer::CloudRenderer(uint32_t width, uint32_t height, const Camera* camera, const Sun* sun, const Wind* wind, const CloudData* cloudData, const Atmosphere* atmosphere, const Precomputer* precomp) :
		m_Width(width),
		m_Height(height),
		m_Camera(camera),
		m_Sun(sun),
		m_Wind(wind),
		m_Precomputer(precomp),
		m_CloudData(cloudData),
		m_Atmosphere(atmosphere),
		m_VertShader("cloud/cloud.vert", false),
		m_FragShader("cloud/cloud.frag", false),
		m_DescriptorPool{VK_NULL_HANDLE}
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Descriptor set layout
		VkDescriptorSetLayoutBinding depthAttBinding;
		depthAttBinding.binding = 0;
		depthAttBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthAttBinding.descriptorCount = 1;
		depthAttBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		depthAttBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.pNext = nullptr;
		descSetLayoutCreateInfo.flags = 0;
		descSetLayoutCreateInfo.bindingCount = 1;
		descSetLayoutCreateInfo.pBindings = &depthAttBinding;

		VkResult result = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout);
		ASSERT_VULKAN(result);

		// other
		CreatePipelineLayout(device);
	}

	void CloudRenderer::Destroy()
	{
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		m_VertShader.Destroy();
		m_FragShader.Destroy();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}

	void CloudRenderer::Resize(uint32_t width, uint32_t height)
	{
		// needed for dynamic viewport/scissor.
		m_Width = width;
		m_Height = height;
	}

	void CloudRenderer::AllocateResources(
		std::vector<VkImageView> &colorImageViews,
		std::vector<VkImageView> &depthImageViews,
		std::vector<VkImageView> &swapchainImageViews,
		std::vector<VkFramebuffer> &framebuffers,
		VkRenderPass renderpass) {

		m_RenderPass = renderpass;
		// destroy resources.
		VkDevice device = VulkanAPI::GetDevice();
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);

		// one descriptor per color- and depth-image.
		const uint32_t imageCount = depthImageViews.size();

		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			.descriptorCount = imageCount
		};

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = imageCount;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		ASSERT_VULKAN(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));

		std::vector<VkDescriptorSetLayout> allocLayouts(imageCount, m_DescriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
			.pSetLayouts = allocLayouts.data()
		};

		m_DescriptorSets.resize(imageCount);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()));

		std::vector<VkDescriptorImageInfo> texInfo(imageCount, {
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL
		});
		std::vector<VkWriteDescriptorSet> writeDescSets(imageCount, {
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
			texInfo[i].imageView = depthImageViews[i];
			writeDescSets[i].pImageInfo = &texInfo[i];

			writeDescSets[i].dstSet = m_DescriptorSets[i];
		}
		vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
	}

	std::pair<VkSubpassDescription, VkSubpassContents> CloudRenderer::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &color_attachment,
		const VkAttachmentReference &depth_attachment,
		const VkAttachmentReference &swapchain_attachment) {

		m_DepthAttachmentReference = {
			.attachment = depth_attachment.attachment,
			// general for both input- and depth-attachment.
			.layout = VK_IMAGE_LAYOUT_GENERAL
		};
		return {
			{
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 1,
				.pInputAttachments = &m_DepthAttachmentReference,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = &m_DepthAttachmentReference,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr,
			},
			VK_SUBPASS_CONTENTS_INLINE
		};

		// VkSubpassDependency subpassDependency;
		// subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		// subpassDependency.dstSubpass = 0;
		// subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// subpassDependency.srcAccessMask = 0;
		// subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		// subpassDependency.dependencyFlags = 0;
	}

	void CloudRenderer::CreatePipelineLayout(VkDevice device)
	{
		std::vector<VkDescriptorSetLayout> descSetLayouts = { 
			Camera::GetDescriptorSetLayout(), 
			m_Sun->GetDescriptorSetLayout(),
			CloudData::GetDescriptorSetLayout(),
			m_DescriptorSetLayout,
			Wind::GetDescriptorSetLayout(),
			m_Atmosphere->GetScatteringSampleDescriptorLayout(),
			m_Atmosphere->GetScatteringSampleDescriptorLayout(),
			m_Atmosphere->GetTransmittanceSampleDescriptorLayout(),
			m_Atmosphere->GetTransmittanceSampleDescriptorLayout(),
			m_Precomputer->GetEffectiveEnvSetLayout(),
			m_Precomputer->GetEffectiveEnvSetLayout(),
			m_Precomputer->GetRatioDescriptorSetLayout() };

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

	void CloudRenderer::CreatePipeline(size_t subpass, VkRenderPass renderpass)
	{
		// Pipeline may have to be recreated, so the subpass is stored.
		// (Recreation only takes place after this is assigned).
		m_Subpass = subpass;

		VkDevice device = VulkanAPI::GetDevice();

		// Vertex shader stage
		VkPipelineShaderStageCreateInfo vertStageCreateInfo;
		vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageCreateInfo.pNext = nullptr;
		vertStageCreateInfo.flags = 0;
		vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageCreateInfo.module = m_VertShader.GetVulkanModule();
		vertStageCreateInfo.pName = "main";
		vertStageCreateInfo.pSpecializationInfo = nullptr;

		// Fragment shader stage
		VkSpecializationMapEntry sampleCountMapEntry;
		sampleCountMapEntry.constantID = 0;
		sampleCountMapEntry.offset = offsetof(CloudSampleCounts, primary);
		sampleCountMapEntry.size = sizeof(CloudSampleCounts::primary);
		
		VkSpecializationMapEntry secondarySampleCountMapEntry;
		secondarySampleCountMapEntry.constantID = 1;
		secondarySampleCountMapEntry.offset = offsetof(CloudSampleCounts, secondary);
		secondarySampleCountMapEntry.size = sizeof(CloudSampleCounts::secondary);

		std::vector<VkSpecializationMapEntry> fragSpecMapEntries = { sampleCountMapEntry, secondarySampleCountMapEntry };

		CloudSampleCounts sampleCounts = m_CloudData->GetSampleCounts();

		VkSpecializationInfo fragSpecInfo;
		fragSpecInfo.mapEntryCount = fragSpecMapEntries.size();
		fragSpecInfo.pMapEntries = fragSpecMapEntries.data();
		fragSpecInfo.dataSize = sizeof(CloudSampleCounts);
		fragSpecInfo.pData = &sampleCounts;

		VkPipelineShaderStageCreateInfo fragStageCreateInfo;
		fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageCreateInfo.pNext = nullptr;
		fragStageCreateInfo.flags = 0;
		fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageCreateInfo.module = m_FragShader.GetVulkanModule();
		fragStageCreateInfo.pName = "main";
		fragStageCreateInfo.pSpecializationInfo = &fragSpecInfo;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages = { vertStageCreateInfo, fragStageCreateInfo };

		// Vertex input
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
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_Width);
		viewport.height = static_cast<float>(m_Height);
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
		depthStencilCreateInfo.depthTestEnable = VK_FALSE;
		depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilCreateInfo.front = {};
		depthStencilCreateInfo.back = {};
		depthStencilCreateInfo.minDepthBounds = 0.0f;
		depthStencilCreateInfo.maxDepthBounds = 1.0f;

		// Color blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
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
		createInfo.renderPass = renderpass;
		createInfo.subpass = subpass;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_Pipeline);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx)
	{
		// Specialization constants
		if (m_CloudData->HaveSampleCountsChanged())
		{
			// Recreate pipline
			VkDevice device = VulkanAPI::GetDevice();
			vkDestroyPipeline(device, m_Pipeline, nullptr);
			CreatePipeline(m_Subpass, m_RenderPass);
		}

		// Bind pipeline
		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

		// Viewport
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_Width);
		viewport.height = static_cast<float>(m_Height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(buf, 0, 1, &viewport);

		// Scissor
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Width, m_Height };

		vkCmdSetScissor(buf, 0, 1, &scissor);

		// Draw
		std::vector<VkDescriptorSet> descSets = { 
			m_Camera->GetDescriptorSet(), 
			m_Sun->GetDescriptorSet(),
			m_CloudData->GetDescriptorSet(),
			m_DescriptorSets[frame_indx],
			m_Wind->GetDescriptorSet(),
			m_Atmosphere->GetScatteringSampleDescriptorSet(0),
			m_Atmosphere->GetScatteringSampleDescriptorSet(1),
			m_Atmosphere->GetTransmittanceSampleDescriptorSet(0),
			m_Atmosphere->GetTransmittanceSampleDescriptorSet(1),
			m_Precomputer->GetEffectiveEnvSet(0),
			m_Precomputer->GetEffectiveEnvSet(1),
			m_Precomputer->GetRatioDescriptorSet() };
		vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDraw(buf, 6, 1, 0, 0);
	}
}
