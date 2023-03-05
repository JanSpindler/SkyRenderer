#pragma once

#include "engine/graphics/vulkan/Swapchain.hpp"
#include <engine/graphics/Common.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <imgui.h>
#include "engine/graphics/Subpass.hpp"

namespace en
{
	// singleton?
	class ImGuiRenderer : public Subpass
	{
	public:
		ImGuiRenderer(size_t maxConcurrent);
		~ImGuiRenderer() override;

		void Resize(uint32_t width, uint32_t height);
		void StartFrame();
		void EndFrame(VkQueue queue, size_t imageIndx);

		void SetBackgroundImageView(VkImageView backgroundImageView);

		std::pair<VkSubpassDescription, VkSubpassContents> GetSubpass(
			size_t subpass_indx,
			const VkAttachmentReference &color_attachment,
			const VkAttachmentReference &depth_attachment,
			const VkAttachmentReference &swapchain_attachment) override;

		void CreatePipeline(size_t subpass, const VkRenderPass renderPass) override;
		void RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) override;

	private:
		size_t m_MaxConcurrent;

		ImDrawData *m_FrameDrawData;

		VkDescriptorPool m_ImGuiDescriptorPool;
		// store for passing to secondary commandBuffer.
		VkRenderPass m_RenderPass;
		uint32_t m_Subpass;
		std::vector<VkFramebuffer> m_Framebuffers;

		void CreateImGuiDescriptorPool(VkDevice device);

		void CreateImageResources(VkDevice device);
		void CreateFramebuffer(VkDevice device);

		void InitImGuiBackend(VkDevice device);
	};
}
