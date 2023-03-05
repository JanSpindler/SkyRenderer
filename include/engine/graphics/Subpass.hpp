#pragma once

#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace en {
	
class Subpass {
	public:
		virtual ~Subpass() {};

		// Have to think about resize, maybe it should be called from outside...
		// virtual void Resize(uint32_t width, uint32_t height);

		// Subpasses may need some resources that require access to the
		// imageViews used in the framebuffer (eg. inputAttachment).
		//
		// add definition, may not be required by subpass.
		virtual void AllocateResources(
			std::vector<VkImageView> &colorImageViews,
			std::vector<VkImageView> &depthImageViews,
			std::vector<VkImageView> &swapchainImageViews,
			std::vector<VkFramebuffer> &framebuffers,
			VkRenderPass renderpass) { }

		// the subpasses may use the attachments differently, so pass them here.
		// Pass by reference so Subpass can pass these to the VkSubpassDescription
		// (which takes pointers to the attachments) without needing to store them
		// somewhere else.
		// As subpasses may record their commandBuffers differently (Inline vs secondary)
		// pass which type is used to the SubpassRenderer here.
		virtual std::pair<VkSubpassDescription, VkSubpassContents> GetSubpass(
			size_t subpass_indx,
			const VkAttachmentReference &color_attachment,
			const VkAttachmentReference &depth_attachment,
			const VkAttachmentReference &swapchain_attachment) = 0;

		// renderPass (or a renderPass compatible with the one in which the pipeline will be used) is
		// needed to create the pipeline.
		virtual void CreatePipeline(size_t subpass, const VkRenderPass renderPass) = 0;

		// frame_indx can be useful if the Subpass requires some images on its own, these would also have to
		// be different for each frame.
		virtual void RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) = 0;
};

}
