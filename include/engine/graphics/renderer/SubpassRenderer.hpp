#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "engine/graphics/Subpass.hpp"
#include "engine/graphics/vulkan/CommandPool.hpp"
#include "engine/graphics/vulkan/Swapchain.hpp"

// shared includes.
#include "renderpass.h"

namespace en {

class SubpassRenderer {
public:
	// create one render-image for each swapchain-image, copy contents of render-image at end of pipeline.
	SubpassRenderer(
		std::vector<std::shared_ptr<Subpass>> subpasses,
		std::vector<VkSubpassDependency> dependencies,
		vk::Swapchain &swapchain);
	~SubpassRenderer();

	void SetAllocateSubpasses(std::vector<std::shared_ptr<Subpass>> subpasses, std::vector<VkSubpassDependency> dependencies);
	void CreateRenderPass();
	void Frame(VkQueue queue, size_t indx, VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore);
	void Resize(uint32_t width, uint32_t height);

private:
	vk::Swapchain &m_Swapchain;
	size_t m_ImageCount;
	std::vector<std::shared_ptr<Subpass>> m_Subpasses;
	std::vector<VkSubpassDependency> m_SubpassDependencies;
	std::vector<VkSubpassContents> m_SubpassContents;

	vk::CommandPool m_CommandPool;
	std::vector<VkCommandBuffer> m_CommandBuffers;

	// need one image per frame in flight (may be concurrent).
	VkFormat m_ColorFormat;
	std::vector<VkDeviceMemory> m_ColorImageMemory;
	std::vector<VkImage> m_ColorImages;
	std::vector<VkImageView> m_ColorImageViews;

	VkFormat m_DepthFormat;
	std::vector<VkDeviceMemory> m_DepthImageMemory;
	std::vector<VkImage> m_DepthImages;
	std::vector<VkImageView> m_DepthImageViews;

	uint32_t m_Width, m_Height;
	std::vector<VkFramebuffer> m_Framebuffers;
	VkRenderPass m_RenderPass;

	void CreateAttachments(VkDevice device);
	void CreateFramebuffers();
	void CreateCommandBuffers();
	void RecordFrameCommandBuffer(size_t indx);
};

}
