#include <engine/graphics/renderer/CloudRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <vector>
#include <engine/graphics/vulkan/CommandRecorder.hpp>
#include <glm/gtc/random.hpp>

namespace en
{
	CloudRenderer::CloudRenderer(uint32_t width, uint32_t height, const Camera* camera, const Sun* sun, const CloudData* cloudData) :
		m_Width(width),
		m_Height(height),
		m_Camera(camera),
		m_Sun(sun),
		m_CloudData(cloudData),
		m_CloudVertShader("cloud/cloud.vert", false),
		m_CloudFragShader("cloud/cloud.frag", false),
		m_CommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, VulkanAPI::GetGraphicsQFI()),
		m_TuRandomBuffer(
			sizeof(float), 
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			{})
	{
		VkDevice device = VulkanAPI::GetDevice();

		FindFormats();

		CreateTuDescriptorResources(device);
		CreateRenderPass(device);
		CreateCloudPipelineLayout(device);
		CreateCloudPipeline(device);
		CreateCommandBuffer();
		CreateColorImageResources(device);
		CreateDepthImageResources(device);
		CreateTuImageResources(device);
		CreateFramebuffer(device);
		RecordCommandBuffer();
	}

	void CloudRenderer::Render(VkQueue queue)
	{
		// Set random value
		float randomVal = glm::linearRand(0.0, 1.0);
		m_TuRandomBuffer.MapMemory(sizeof(float), &randomVal, 0, 0);

		// Specialization constants
		if (m_CloudData->HaveSampleCountsChanged())
		{
			// Recreate pipline
			VkDevice device = VulkanAPI::GetDevice();
			vkDestroyPipeline(device, m_CloudPipeline, nullptr);
			CreateCloudPipeline(device);

			// Rerecord command buffer
			RecordCommandBuffer();
		}

		// Submit render task
		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		VkResult result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::Destroy()
	{
		VkDevice device = VulkanAPI::GetDevice();

		m_CommandPool.Destroy();
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);

		vkDestroyImageView(device, m_TuImageView, nullptr);
		vkFreeMemory(device, m_TuImageMemory, nullptr);
		vkDestroyImage(device, m_TuImage, nullptr);

		vkDestroyImageView(device, m_DepthImageView, nullptr);
		vkFreeMemory(device, m_DepthImageMemory, nullptr);
		vkDestroyImage(device, m_DepthImage, nullptr);

		vkDestroyImageView(device, m_ColorImageView, nullptr);
		vkFreeMemory(device, m_ColorImageMemory, nullptr);
		vkDestroyImage(device, m_ColorImage, nullptr);

		vkDestroyPipelineLayout(device, m_CloudPipelineLayout, nullptr);
		vkDestroyPipeline(device, m_CloudPipeline, nullptr);
		m_CloudVertShader.Destroy();
		m_CloudFragShader.Destroy();
		vkDestroyRenderPass(device, m_RenderPass, nullptr);

		vkDestroyDescriptorPool(device, m_TuDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_TuDescriptorSetLayout, nullptr);
		vkDestroySampler(device, m_TuSampler, nullptr);

		m_TuRandomBuffer.Destroy();
	}

	void CloudRenderer::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;
		VkDevice device = VulkanAPI::GetDevice();

		// Destroy
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);

		vkDestroyImageView(device, m_TuImageView, nullptr);
		vkFreeMemory(device, m_TuImageMemory, nullptr);
		vkDestroyImage(device, m_TuImage, nullptr);

		vkDestroyImageView(device, m_DepthImageView, nullptr);
		vkFreeMemory(device, m_DepthImageMemory, nullptr);
		vkDestroyImage(device, m_DepthImage, nullptr);

		vkDestroyImageView(device, m_ColorImageView, nullptr);
		vkFreeMemory(device, m_ColorImageMemory, nullptr);
		vkDestroyImage(device, m_ColorImage, nullptr);

		// Create
		CreateColorImageResources(device);
		CreateDepthImageResources(device);
		CreateTuImageResources(device);
		CreateFramebuffer(device);
		RecordCommandBuffer();
	}

	VkImageView CloudRenderer::GetImageView() const
	{
		return m_ColorImageView;
	}

	void CloudRenderer::FindFormats()
	{
		// Color Format
		m_ColorFormat = VulkanAPI::GetSurfaceFormat().format;
		m_TuFormat = m_ColorFormat;

		// Depth Format
		std::vector<VkFormat> depthFormatCandidates = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT };
		m_DepthFormat = VulkanAPI::FindSupportedFormat(
			depthFormatCandidates,
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		if (m_DepthFormat == VK_FORMAT_UNDEFINED)
			Log::Error("Failed to find Depth Format for SimpleModelRenderer", true);
	}

	void CloudRenderer::CreateTuDescriptorResources(VkDevice device)
	{
		// Create sampler
		VkSamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.pNext = nullptr;
		samplerCreateInfo.flags = 0;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 0.0f;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		VkResult result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_TuSampler);
		ASSERT_VULKAN(result);

		// Create descriptor set layout
		VkDescriptorSetLayoutBinding tuImageBinding;
		tuImageBinding.binding = 0;
		tuImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		tuImageBinding.descriptorCount = 1;
		tuImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		tuImageBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding tuRandomUniformBinding;
		tuRandomUniformBinding.binding = 1;
		tuRandomUniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		tuRandomUniformBinding.descriptorCount = 1;
		tuRandomUniformBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		tuRandomUniformBinding.pImmutableSamplers = nullptr;

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { tuImageBinding, tuRandomUniformBinding };

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.bindingCount = layoutBindings.size();
		layoutCreateInfo.pBindings = layoutBindings.data();

		result = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_TuDescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create descriptor pool
		VkDescriptorPoolSize tuImagePoolSize;
		tuImagePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		tuImagePoolSize.descriptorCount = 1;

		VkDescriptorPoolSize tuRandomUniformPoolSize;
		tuRandomUniformPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		tuRandomUniformPoolSize.descriptorCount = 1;

		std::vector<VkDescriptorPoolSize> poolSizes = { tuImagePoolSize, tuRandomUniformPoolSize };

		VkDescriptorPoolCreateInfo poolCreateInfo;
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.pNext = nullptr;
		poolCreateInfo.flags = 0;
		poolCreateInfo.maxSets = 1;
		poolCreateInfo.poolSizeCount = poolSizes.size();
		poolCreateInfo.pPoolSizes = poolSizes.data();

		result = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &m_TuDescriptorPool);
		ASSERT_VULKAN(result);

		// Allocate descriptor set
		VkDescriptorSetAllocateInfo setAllocateInfo;
		setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		setAllocateInfo.pNext = nullptr;
		setAllocateInfo.descriptorPool = m_TuDescriptorPool;
		setAllocateInfo.descriptorSetCount = 1;
		setAllocateInfo.pSetLayouts = &m_TuDescriptorSetLayout;

		result = vkAllocateDescriptorSets(device, &setAllocateInfo, &m_TuDescriptorSet);
		ASSERT_VULKAN(result);

		// Write uniform buffer
		VkDescriptorBufferInfo tuRandomUniformBufferInfo;
		tuRandomUniformBufferInfo.buffer = m_TuRandomBuffer.GetVulkanHandle();
		tuRandomUniformBufferInfo.offset = 0;
		tuRandomUniformBufferInfo.range = sizeof(float);

		VkWriteDescriptorSet tuRandomUniformWrite;
		tuRandomUniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		tuRandomUniformWrite.pNext = nullptr;
		tuRandomUniformWrite.dstSet = m_TuDescriptorSet;
		tuRandomUniformWrite.dstBinding = 1;
		tuRandomUniformWrite.dstArrayElement = 0;
		tuRandomUniformWrite.descriptorCount = 1;
		tuRandomUniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		tuRandomUniformWrite.pImageInfo = nullptr;
		tuRandomUniformWrite.pBufferInfo = &tuRandomUniformBufferInfo;
		tuRandomUniformWrite.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &tuRandomUniformWrite, 0, nullptr);
	}

	void CloudRenderer::CreateRenderPass(VkDevice device)
	{
		// Depth attachment
		VkAttachmentDescription depthAttachment;
		depthAttachment.flags = 0;
		depthAttachment.format = m_DepthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference;
		depthAttachmentReference.attachment = 0;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Color attachment
		VkAttachmentDescription color1Attachment;
		color1Attachment.flags = 0;
		color1Attachment.format = VulkanAPI::GetSurfaceFormat().format;
		color1Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color1Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color1Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color1Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color1Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color1Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color1Attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentReference color1AttachmentReference;
		color1AttachmentReference.attachment = 1;
		color1AttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentDescription> attachments = { depthAttachment, color1Attachment };

		// Subpass
		VkSubpassDescription cloudSubpass;
		cloudSubpass.flags = 0;
		cloudSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		cloudSubpass.inputAttachmentCount = 0;
		cloudSubpass.pInputAttachments = nullptr;
		cloudSubpass.colorAttachmentCount = 1;
		cloudSubpass.pColorAttachments = &color1AttachmentReference;
		cloudSubpass.pResolveAttachments = nullptr;
		cloudSubpass.pDepthStencilAttachment = &depthAttachmentReference;
		cloudSubpass.preserveAttachmentCount = 0;
		cloudSubpass.pPreserveAttachments = nullptr;

		std::vector<VkSubpassDescription> subpasses = { cloudSubpass };

		// Subpass dependency
		VkSubpassDependency cloudSubpassDependency;
		cloudSubpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		cloudSubpassDependency.dstSubpass = 0;
		cloudSubpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		cloudSubpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		cloudSubpassDependency.srcAccessMask = 0;
		cloudSubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		cloudSubpassDependency.dependencyFlags = 0;

		std::vector<VkSubpassDependency> subpassDependencies = { cloudSubpassDependency };

		// Create render pass
		VkRenderPassCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.attachmentCount = attachments.size();
		createInfo.pAttachments = attachments.data();
		createInfo.subpassCount = subpasses.size();
		createInfo.pSubpasses = subpasses.data();
		createInfo.dependencyCount = subpassDependencies.size();
		createInfo.pDependencies = subpassDependencies.data();

		VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, &m_RenderPass);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateCloudPipelineLayout(VkDevice device)
	{
		std::vector<VkDescriptorSetLayout> descSetLayouts = { 
			Camera::GetDescriptorSetLayout(), 
			m_Sun->GetDescriptorSetLayout(),
			CloudData::GetDescriptorSetLayout(),
			m_TuDescriptorSetLayout};

		VkPipelineLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.setLayoutCount = descSetLayouts.size();
		layoutCreateInfo.pSetLayouts = descSetLayouts.data();
		layoutCreateInfo.pushConstantRangeCount = 0;
		layoutCreateInfo.pPushConstantRanges = nullptr;

		VkResult result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &m_CloudPipelineLayout);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateCloudPipeline(VkDevice device)
	{
		// Vertex shader stage
		VkPipelineShaderStageCreateInfo vertStageCreateInfo;
		vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageCreateInfo.pNext = nullptr;
		vertStageCreateInfo.flags = 0;
		vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageCreateInfo.module = m_CloudVertShader.GetVulkanModule();
		vertStageCreateInfo.pName = "main";
		vertStageCreateInfo.pSpecializationInfo = nullptr;

		// Fragment shader stage
		VkSpecializationMapEntry sampleCountMapEntry;
		sampleCountMapEntry.constantID = 0;
		sampleCountMapEntry.offset = offsetof(CloudSampleCounts, CloudSampleCounts::primary);
		sampleCountMapEntry.size = sizeof(CloudSampleCounts::primary);
		
		VkSpecializationMapEntry secondarySampleCountMapEntry;
		secondarySampleCountMapEntry.constantID = 1;
		secondarySampleCountMapEntry.offset = offsetof(CloudSampleCounts, CloudSampleCounts::secondary);
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
		fragStageCreateInfo.module = m_CloudFragShader.GetVulkanModule();
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
		colorBlendAttachment.blendEnable = VK_TRUE;
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
		createInfo.layout = m_CloudPipelineLayout;
		createInfo.renderPass = m_RenderPass;
		createInfo.subpass = 0;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_CloudPipeline);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateColorImageResources(VkDevice device)
	{
		// Create Image
		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_ColorFormat;
		imageCreateInfo.extent = { m_Width, m_Height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // TODO: maybe?

		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &m_ColorImage);
		ASSERT_VULKAN(result);

		// Image Memory
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, m_ColorImage, &memoryRequirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		result = vkAllocateMemory(device, &allocateInfo, nullptr, &m_ColorImageMemory);
		ASSERT_VULKAN(result);

		result = vkBindImageMemory(device, m_ColorImage, m_ColorImageMemory, 0);
		ASSERT_VULKAN(result);

		// Create image view
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_ColorImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_ColorFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ColorImageView);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateDepthImageResources(VkDevice device)
	{
		// Create image
		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_DepthFormat;
		imageCreateInfo.extent = { m_Width, m_Height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &m_DepthImage);
		ASSERT_VULKAN(result);

		// Image Memory
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, m_DepthImage, &memoryRequirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		result = vkAllocateMemory(device, &allocateInfo, nullptr, &m_DepthImageMemory);
		ASSERT_VULKAN(result);

		result = vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0);
		ASSERT_VULKAN(result);

		// Create image view
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_DepthImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_DepthFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_DepthImageView);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateTuImageResources(VkDevice device)
	{
		// Create Image
		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_TuFormat;
		imageCreateInfo.extent = { m_Width, m_Height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // TODO: maybe?

		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &m_TuImage);
		ASSERT_VULKAN(result);

		// Image Memory
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, m_TuImage, &memoryRequirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		result = vkAllocateMemory(device, &allocateInfo, nullptr, &m_TuImageMemory);
		ASSERT_VULKAN(result);

		result = vkBindImageMemory(device, m_TuImage, m_TuImageMemory, 0);
		ASSERT_VULKAN(result);

		// Create image view
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_TuImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_TuFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_TuImageView);
		ASSERT_VULKAN(result);

		// Transfer to general layout
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		result = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
		ASSERT_VULKAN(result);

		vk::CommandRecorder::ImageLayoutTransfer(
			m_CommandBuffer,
			m_TuImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_NONE_KHR,
			VK_ACCESS_NONE_KHR,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		result = vkEndCommandBuffer(m_CommandBuffer);
		ASSERT_VULKAN(result);

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		VkQueue queue = VulkanAPI::GetGraphicsQueue();
		result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		ASSERT_VULKAN(result);
		result = vkQueueWaitIdle(queue);
		ASSERT_VULKAN(result);

		// Update descriptor set
		VkDescriptorImageInfo tuImageInfo;
		tuImageInfo.sampler = m_TuSampler;
		tuImageInfo.imageView = m_TuImageView;
		tuImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet tuImageWrite;
		tuImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		tuImageWrite.pNext = nullptr;
		tuImageWrite.dstSet = m_TuDescriptorSet;
		tuImageWrite.dstBinding = 0;
		tuImageWrite.dstArrayElement = 0;
		tuImageWrite.descriptorCount = 1;
		tuImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		tuImageWrite.pImageInfo = &tuImageInfo;
		tuImageWrite.pBufferInfo = nullptr;
		tuImageWrite.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &tuImageWrite, 0, nullptr);
	}

	void CloudRenderer::CreateFramebuffer(VkDevice device)
	{
		std::vector<VkImageView> attachments = { m_DepthImageView, m_ColorImageView };

		VkFramebufferCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.renderPass = m_RenderPass;
		createInfo.attachmentCount = attachments.size();
		createInfo.pAttachments = attachments.data();
		createInfo.width = m_Width;
		createInfo.height = m_Height;
		createInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(device, &createInfo, nullptr, &m_Framebuffer);
		ASSERT_VULKAN(result);
	}

	void CloudRenderer::CreateCommandBuffer()
	{
		m_CommandPool.AllocateBuffers(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		m_CommandBuffer = m_CommandPool.GetBuffer(0);
	}

	void CloudRenderer::RecordCommandBuffer()
	{
		// Begin command buffer
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		VkResult result = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
		ASSERT_VULKAN(result);

		// Begin render pass
		VkClearValue depthClearValue = { 1.0f, 0 };
		VkClearValue color1ClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
		std::vector<VkClearValue> clearValues = { depthClearValue, color1ClearValue };

		VkRenderPassBeginInfo renderPassBeginInfo;
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = m_RenderPass;
		renderPassBeginInfo.framebuffer = m_Framebuffer;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = { m_Width, m_Height };
		renderPassBeginInfo.clearValueCount = clearValues.size();
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(m_CommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		// Bind pipeline
		vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CloudPipeline);

		// Viewport
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_Width);
		viewport.height = static_cast<float>(m_Height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

		// Scissor
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { m_Width, m_Height };

		vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

		// Draw
		std::vector<VkDescriptorSet> descSets = { 
			m_Camera->GetDescriptorSet(), 
			m_Sun->GetDescriptorSet(),
			m_CloudData->GetDescriptorSet(),
			m_TuDescriptorSet};
		vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CloudPipelineLayout, 0, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDraw(m_CommandBuffer, 6, 1, 0, 0);

		// End render pass
		vkCmdEndRenderPass(m_CommandBuffer);

		// Copy to tu image
		/*VkImageCopy imageCopy;
		imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.srcSubresource.mipLevel = 0;
		imageCopy.srcSubresource.baseArrayLayer = 0;
		imageCopy.srcSubresource.layerCount = 1;
		imageCopy.srcOffset = { 0, 0, 0 };
		imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.dstSubresource.mipLevel = 0;
		imageCopy.dstSubresource.baseArrayLayer = 0;
		imageCopy.dstSubresource.layerCount = 1;
		imageCopy.dstOffset = { 0, 0, 0 };
		imageCopy.extent = { m_Width, m_Height, 1 };

		vkCmdCopyImage(
			m_CommandBuffer,
			m_ColorImage,
			VK_IMAGE_LAYOUT_GENERAL,
			m_TuImage,
			VK_IMAGE_LAYOUT_GENERAL,
			1,
			&imageCopy);*/

		// End command buffer
		result = vkEndCommandBuffer(m_CommandBuffer);
		ASSERT_VULKAN(result);
	}
}
