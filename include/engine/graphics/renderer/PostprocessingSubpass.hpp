#pragma once

#include "engine/graphics/vulkan/Swapchain.hpp"
#include <engine/graphics/Common.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <engine/graphics/Subpass.hpp>
#include <engine/objects/Model.hpp>
#include <engine/graphics/Camera.hpp>
#include <vulkan/vulkan_core.h>

namespace en
{
	class PostprocessingSubpass : public Subpass {
	public:
		PostprocessingSubpass(uint32_t width, uint32_t height);
		~PostprocessingSubpass() override;

		void CreatePipeline(size_t subpass, const VkRenderPass renderPass) override;
		void Resize(uint32_t width, uint32_t height);

		std::pair<VkSubpassDescription, VkSubpassContents> GetSubpass(
			size_t subpass_indx,
			const VkAttachmentReference &color_attachment,
			const VkAttachmentReference &depth_attachment,
			const VkAttachmentReference &swapchain_attachment) override;

		void AllocateResources(
			std::vector<VkImageView> &colorImageViews,
			std::vector<VkImageView> &depthImageViews,
			std::vector<VkImageView> &swapchainImageViews,
			std::vector<VkFramebuffer> &framebuffers,
			VkRenderPass renderpass) override;

		void RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) override;

	private:
		vk::Shader m_VertShader;
		vk::Shader m_FragShader;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		VkDescriptorPool m_DescriptorPool;
		VkDescriptorSetLayout m_InputAttachmentDSLayout;
		std::vector<VkDescriptorSet> m_InputAttachmentDSs;

		uint32_t m_Width, m_Height;

		// store here temporarily.
		VkAttachmentReference m_InputAttachmentRef;

		void CreateLayouts(VkDevice device);
	};
}
