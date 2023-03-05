#pragma once

#include "engine/graphics/vulkan/Swapchain.hpp"
#include <engine/graphics/Common.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <engine/graphics/Subpass.hpp>
#include <engine/objects/Model.hpp>
#include <engine/graphics/Camera.hpp>

namespace en
{
	class SimpleModelRenderer : public Subpass {
	public:
		SimpleModelRenderer(uint32_t width, uint32_t height, const Camera* camera, size_t max_concurrent);

		void Render(VkQueue queue, size_t imageIndx) const;
		void Destroy();

		void ResizeFrame(uint32_t width, uint32_t height);

		void AddModelInstance(const ModelInstance* modelInstance);
		void RemoveModelInstance(const ModelInstance* modelInstance);

		std::vector<VkImage> GetColorImages() const;
		std::vector<VkImageView> GetColorImageViews() const;

		void SetImGuiCommandBuffer(VkCommandBuffer imGuiCommandBuffer);

		void RecordCommandBuffers();

	private:
		size_t m_MaxConcurrent;

		std::vector<const ModelInstance*> m_ModelInstances;

		uint32_t m_FrameWidth;
		uint32_t m_FrameHeight;
		const Camera* m_Camera;

		VkRenderPass m_RenderPass;
		uint32_t m_Subpass;
		std::vector<VkFramebuffer> m_Framebuffers;

		vk::Shader m_VertShader;
		vk::Shader m_FragShader;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		vk::CommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;

		// void FindFormats();
		void CreatePipelineLayout(VkDevice device);
		void CreatePipeline(size_t subpass, const VkRenderPass renderPass) override;
		void CreateCommandBuffers();

		std::pair<VkSubpassDescription, VkSubpassContents> GetSubpass(
			size_t subpass_indx,
			const VkAttachmentReference &color_attachment,
			const VkAttachmentReference &depth_attachment,
			const VkAttachmentReference &swapchain_attachment) override;

		void RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) override;

		void AllocateResources(
			std::vector<VkImageView> &colorImageViews,
			std::vector<VkImageView> &depthImageViews,
			std::vector<VkImageView> &swapchainImageViews,
			std::vector<VkFramebuffer> &framebuffers,
			VkRenderPass renderpass) override;
	};
}
