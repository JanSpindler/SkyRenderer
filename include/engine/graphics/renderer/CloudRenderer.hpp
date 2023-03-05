#pragma once

#include "engine/graphics/Atmosphere.hpp"
#include "engine/graphics/Precomputer.hpp"
#include "engine/graphics/Subpass.hpp"
#include <engine/graphics/Common.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <engine/graphics/Camera.hpp>
#include <engine/objects/CloudData.hpp>
#include <engine/graphics/Sun.hpp>
#include <engine/objects/Wind.hpp>

namespace en
{
	class CloudRenderer : public Subpass
	{
	public:
	CloudRenderer(uint32_t width, uint32_t height, const Camera* camera, const Sun* sun, const Wind* wind, const CloudData* cloudData, const Atmosphere* atmosphere, const Precomputer* precomp);

		void Render(VkQueue queue);
		void Destroy();

		void Resize(uint32_t width, uint32_t height);

		void AllocateResources(
			std::vector<VkImageView> &colorImageViews,
			std::vector<VkImageView> &depthImageViews,
			std::vector<VkImageView> &swapchainImageViews,
			std::vector<VkFramebuffer> &framebuffers,
			VkRenderPass renderpass) override;

		void CreatePipeline(size_t subpass, VkRenderPass renderpass) override;
		void RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) override;
		std::pair<VkSubpassDescription, VkSubpassContents> GetSubpass(
			size_t subpass_indx,
			const VkAttachmentReference &color_attachment,
			const VkAttachmentReference &depth_attachment,
			const VkAttachmentReference &swapchain_attachment) override;

	private:
		uint32_t m_Width;
		uint32_t m_Height;
		const Camera* m_Camera;
		const Sun* m_Sun;
		const Wind* m_Wind;
		const CloudData* m_CloudData;
		const Atmosphere* m_Atmosphere;
		const Precomputer* m_Precomputer;

		VkAttachmentReference m_DepthAttachmentReference;

		VkDescriptorPool m_DescriptorPool;
		VkDescriptorSetLayout m_DescriptorSetLayout;
		std::vector<VkDescriptorSet> m_DescriptorSets;

		VkRenderPass m_RenderPass;
		size_t m_Subpass;
		vk::Shader m_VertShader;
		vk::Shader m_FragShader;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		void CreateRenderPass(VkDevice device);
		void CreatePipelineLayout(VkDevice device);
	};
}
