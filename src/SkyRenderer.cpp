#include "engine/graphics/Common.hpp"
#include <engine/graphics/renderer/SkyRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>
#include <set>

#include <scattering.h>
#include <sky.h>

namespace en
{
	SkyRenderer::SkyRenderer(
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
		m_VertShader("sky/atmosphere.vert", false),
		m_FragShader("sky/atmosphere.frag", false),
		m_GraphicsCommandPool(0, VulkanAPI::GetGraphicsQFI()),
		m_Atmosphere(atmosphere),
		m_Precomp(precomp),
		m_Aerial{aerial},
		m_MaxConcurrent{max_concurrent},
		m_GraphicsPipeline{VK_NULL_HANDLE}
	{
		VkDevice device = VulkanAPI::GetDevice();

		CreateCommandBuffers();
		CreatePipelineLayout(device);
	}

	void SkyRenderer::Destroy()
	{
		VkDevice device = VulkanAPI::GetDevice();

		m_GraphicsCommandPool.Destroy();

		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_GraphicsPipeline, nullptr);

		m_VertShader.Destroy();
		m_FragShader.Destroy();
	}

	void SkyRenderer::ResizeFrame(uint32_t width, uint32_t height)
	{
		m_FrameWidth = width;
		m_FrameHeight = height;
	}

	void SkyRenderer::CreatePipelineLayout(VkDevice device)
	{
		std::vector<VkDescriptorSetLayout> descSetLayouts;
		descSetLayouts.resize(7);
		descSetLayouts[SKY_SETS_CAM] = Camera::GetDescriptorSetLayout();
		descSetLayouts[SKY_SETS_SUN] = m_Sun.GetDescriptorSetLayout();
		descSetLayouts[SKY_SETS_SCTEXTURE0] = m_Atmosphere.GetScatteringSampleDescriptorLayout();
		descSetLayouts[SKY_SETS_SCTEXTURE1] = m_Atmosphere.GetScatteringSampleDescriptorLayout();
		descSetLayouts[SKY_SETS_RATIO] = m_Precomp.GetRatioDescriptorSetLayout();
		descSetLayouts[SKY_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSetLayout();
		descSetLayouts[SKY_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSetLayout();

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

	std::pair<VkSubpassDescription, VkSubpassContents> SkyRenderer::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &color_attachment,
		const VkAttachmentReference &depth_attachment,
		const VkAttachmentReference &swapchain_attachment) {

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
			VK_SUBPASS_CONTENTS_INLINE
		};
	}

	void SkyRenderer::CreatePipeline(size_t subpass, VkRenderPass renderPass)
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
		// no need to write (quad on far plane).
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		// make quad on far plane pass.
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

	void SkyRenderer::CreateCommandBuffers()
	{
		// one buffer per swapchain-image.
		m_GraphicsCommandPool.AllocateBuffers(m_MaxConcurrent, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		m_GraphicsCommandBuffers = m_GraphicsCommandPool.GetBuffers();
	}

	void SkyRenderer::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx)
	{
		// Bind pipeline
		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
		std::vector<VkDescriptorSet> sets(7);
		sets[SKY_SETS_CAM] = m_Camera->GetDescriptorSet();
		sets[SKY_SETS_SUN] = m_Sun.GetDescriptorSet();
		sets[SKY_SETS_SCTEXTURE0] = m_Atmosphere.GetScatteringSampleDescriptorSet(0);
		sets[SKY_SETS_SCTEXTURE1] = m_Atmosphere.GetScatteringSampleDescriptorSet(1);
		sets[SKY_SETS_RATIO] = m_Precomp.GetRatioDescriptorSet();
		sets[SKY_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSet();
		sets[SKY_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSet();

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
