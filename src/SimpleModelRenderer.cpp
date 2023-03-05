#include <engine/graphics/renderer/SimpleModelRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace en
{
	SimpleModelRenderer::SimpleModelRenderer(uint32_t width, uint32_t height, const Camera* camera, size_t max_concurrent) :
		m_FrameWidth(width),
		m_FrameHeight(height),
		m_Camera(camera),
		m_VertShader("simple_material/simple_material.vert", false),
		m_FragShader("simple_material/simple_material.frag", false),
		m_CommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, VulkanAPI::GetGraphicsQFI()),
		m_MaxConcurrent{max_concurrent},
		m_Pipeline{VK_NULL_HANDLE} {
		VkDevice device = VulkanAPI::GetDevice();

		CreatePipelineLayout(device);
		CreateCommandBuffers();
	}

	void SimpleModelRenderer::Render(VkQueue queue, size_t imageIndx) const
	{
		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffers[imageIndx];
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		VkResult result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		ASSERT_VULKAN(result);
	}

	void SimpleModelRenderer::Destroy()
	{
		VkDevice device = VulkanAPI::GetDevice();

		m_CommandPool.Destroy();

		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		m_VertShader.Destroy();
		m_FragShader.Destroy();
	}

	// no longer need to do anything on resize, commandBuffer will be re-recorded by
	// SubpassRenderer.
	void SimpleModelRenderer::ResizeFrame(uint32_t width, uint32_t height) {
		m_FrameWidth = width;
		m_FrameHeight = height;
	}

	void SimpleModelRenderer::AddModelInstance(const ModelInstance* meshInstance)
	{
		m_ModelInstances.push_back(meshInstance);
		RecordCommandBuffers();
	}

	// don't call this
	// TODO: to re-enable this function, the pipeline has to be run in a secondary buffer
	// that is called from the primary buffer, which is passed to the SubpassRenderer.
	void SimpleModelRenderer::RemoveModelInstance(const ModelInstance* modelInstance)
	{
		for (uint32_t i = 0; i < m_ModelInstances.size(); i++)
		{
			if (m_ModelInstances[i] == modelInstance)
				m_ModelInstances.erase(m_ModelInstances.begin() + i);
		}
		RecordCommandBuffers();
	}

	void SimpleModelRenderer::AllocateResources(
		std::vector<VkImageView> &colorImageViews,
		std::vector<VkImageView> &depthImageViews,
		std::vector<VkImageView> &swapchainImageViews,
		std::vector<VkFramebuffer> &framebuffers,
		VkRenderPass renderpass) {

		m_RenderPass = renderpass;
		m_Framebuffers = framebuffers;

		RecordCommandBuffers();
	}

	std::pair<VkSubpassDescription, VkSubpassContents> SimpleModelRenderer::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &color_attachment,
		const VkAttachmentReference &depth_attachment,
		const VkAttachmentReference &swapchain_attachment) {

		m_Subpass = subpass_indx;

		return {
			VkSubpassDescription {
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = nullptr,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = &depth_attachment,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr,
			},
			VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
		};
	}

	void SimpleModelRenderer::CreateCommandBuffers() {
		m_CommandPool.AllocateBuffers(m_MaxConcurrent, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		m_CommandBuffers = m_CommandPool.GetBuffers();
	}

	void SimpleModelRenderer::RecordCommandBuffers() {
		VkCommandBufferInheritanceInfo info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext = nullptr,
			.renderPass = m_RenderPass,
			.subpass = m_Subpass,
			.occlusionQueryEnable = VK_FALSE,
			.queryFlags = 0,
			.pipelineStatistics = 0
		};

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		beginInfo.pInheritanceInfo = &info;

		for (int frame_indx = 0; frame_indx != m_MaxConcurrent; ++frame_indx) {
			vkResetCommandBuffer(m_CommandBuffers[frame_indx], 0);

			info.framebuffer = m_Framebuffers[frame_indx];
			ASSERT_VULKAN(vkBeginCommandBuffer(m_CommandBuffers[frame_indx], &beginInfo));

			// Bind pipeline
			vkCmdBindPipeline(m_CommandBuffers[frame_indx], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

			// Viewport
			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = static_cast<float>(m_FrameHeight);
			viewport.width = static_cast<float>(m_FrameWidth);
			viewport.height = -static_cast<float>(m_FrameHeight);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(m_CommandBuffers[frame_indx], 0, 1, &viewport);

			// Scissor
			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { m_FrameWidth, m_FrameHeight };

			vkCmdSetScissor(m_CommandBuffers[frame_indx], 0, 1, &scissor);

			// Render Model Instances
			VkDeviceSize offsets[] = { 0 };
			// TODO: one camera-set per concurrent frame.
			VkDescriptorSet cameraDescSet = m_Camera->GetDescriptorSet();
			std::vector<VkDescriptorSet> descSets = { 0, cameraDescSet, 0 };
			for (const ModelInstance* modelInstance : m_ModelInstances)
			{
				const Model* model = modelInstance->GetModel();
				for (uint32_t i = 0; i < model->GetMeshCount(); i++)
				{
					const Mesh* mesh = model->GetMesh(i);

					// Descriptor Sets
					descSets[0] = modelInstance->GetDescriptorSet();
					descSets[2] = mesh->GetMaterial()->GetDescriptorSet();
					vkCmdBindDescriptorSets(m_CommandBuffers[frame_indx], VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, descSets.size(), descSets.data(), 0, nullptr);

					// Draw Mesh
					VkBuffer vertexBuffer = mesh->GetVertexBufferVulkanHandle();
					VkBuffer indexBuffer = mesh->GetIndexBufferVulkanHandle();
					uint32_t indexCount = mesh->GetIndexCount();

					vkCmdBindVertexBuffers(m_CommandBuffers[frame_indx], 0, 1, &vertexBuffer, offsets);
					vkCmdBindIndexBuffer(m_CommandBuffers[frame_indx], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(m_CommandBuffers[frame_indx], indexCount, 1, 0, 0, 0);
				}
			}
			vkEndCommandBuffer(m_CommandBuffers[frame_indx]);
		}
	}

	void SimpleModelRenderer::CreatePipelineLayout(VkDevice device)
	{
		VkDescriptorSetLayout modelDescSetLayout = ModelInstance::GetDescriptorSetLayout();
		VkDescriptorSetLayout cameraDescSetLayout = Camera::GetDescriptorSetLayout();
		VkDescriptorSetLayout materialDescSetLayout = Material::GetDescriptorSetLayout();
		std::vector<VkDescriptorSetLayout> descSetLayouts = { modelDescSetLayout, cameraDescSetLayout, materialDescSetLayout };

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

	void SimpleModelRenderer::CreatePipeline(size_t subpass, VkRenderPass renderPass)
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

		// Vertex input
		VkVertexInputBindingDescription bindingDesc = PNTVertex::GetBindingDescription();
		std::array<VkVertexInputAttributeDescription, 3> attrDesc = PNTVertex::GetAttributeDescription();

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo;
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.pNext = nullptr;
		vertexInputCreateInfo.flags = 0;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDesc;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = attrDesc.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = attrDesc.data();

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

	void SimpleModelRenderer::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx)
	{
		vkCmdExecuteCommands(buf, 1, &m_CommandBuffers[frame_indx]);
	}
}
