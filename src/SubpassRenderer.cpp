#include "engine/graphics/renderer/SubpassRenderer.hpp"
#include "engine/graphics/VulkanAPI.hpp"
#include <vulkan/vulkan_core.h>

namespace en {

SubpassRenderer::SubpassRenderer(
	std::vector<std::shared_ptr<Subpass>> subpasses,
	std::vector<VkSubpassDependency> dependencies,
	vk::Swapchain &swapchain ) :

	m_Swapchain{swapchain},
	m_ImageCount{swapchain.GetImageCount()},
	m_ColorImages(m_ImageCount, VK_NULL_HANDLE),
	m_ColorImageViews(m_ImageCount, VK_NULL_HANDLE),
	m_ColorImageMemory(m_ImageCount, VK_NULL_HANDLE),
	m_DepthImages(m_ImageCount, VK_NULL_HANDLE),
	m_DepthImageViews(m_ImageCount, VK_NULL_HANDLE),
	m_DepthImageMemory(m_ImageCount, VK_NULL_HANDLE),
	m_Framebuffers(m_ImageCount, VK_NULL_HANDLE),
	// the command buffers will be used once for image transitions and then for rendering.
	m_CommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, VulkanAPI::GetGraphicsQFI()),
	// reserve space in m_SubpassContents.
	m_SubpassContents(subpasses.size()),
	m_Subpasses{subpasses},
	m_SubpassDependencies{dependencies},
	m_RenderPass{VK_NULL_HANDLE} {

	VkDevice device = VulkanAPI::GetDevice();
	m_ImageCount = m_Swapchain.GetImageCount();
	m_Width = m_Swapchain.m_Width;
	m_Height = m_Swapchain.m_Height;

	CreateCommandBuffers();
	CreateAttachments(device);

	CreateRenderPass();
	CreateFramebuffers();
	auto swapchainColorImageViews = m_Swapchain.GetColorImageViews();
	for (auto sp : m_Subpasses)
		sp->AllocateResources(m_ColorImageViews, m_DepthImageViews, swapchainColorImageViews, m_Framebuffers, m_RenderPass);
}

SubpassRenderer::~SubpassRenderer() {
	VkDevice device = VulkanAPI::GetDevice();
	m_CommandPool.Destroy();

	for (int i = 0; i != m_ColorImages.size(); ++i) {
		vkFreeMemory(device, m_ColorImageMemory[i], nullptr);
		vkDestroyImage(device, m_ColorImages[i], nullptr);
		vkDestroyImageView(device, m_ColorImageViews[i], nullptr);
	}

	for (int i = 0; i != m_DepthImages.size(); ++i) {
		vkFreeMemory(device, m_DepthImageMemory[i], nullptr);
		vkDestroyImage(device, m_DepthImages[i], nullptr);
		vkDestroyImageView(device, m_DepthImageViews[i], nullptr);
	}
	for (int i = 0; i != m_Framebuffers.size(); ++i) {
		vkDestroyFramebuffer(device, m_Framebuffers[i], nullptr);
	}

	vkDestroyRenderPass(device, m_RenderPass, nullptr);
}

void SubpassRenderer::Resize(uint32_t width, uint32_t height) {
	m_Width = width;
	m_Height = height;

	CreateAttachments(VulkanAPI::GetDevice());
	CreateRenderPass();
	CreateFramebuffers();
	auto swapchainColorImageViews = m_Swapchain.GetColorImageViews();
	for (auto sp : m_Subpasses)
		sp->AllocateResources(m_ColorImageViews, m_DepthImageViews, swapchainColorImageViews, m_Framebuffers, m_RenderPass);
}

void SubpassRenderer::CreateAttachments(VkDevice device) {
	// destroy old (or VK_NULL_HANDLE) images+memory.
	for (int i = 0; i != m_ColorImages.size(); ++i) {
		vkFreeMemory(device, m_ColorImageMemory[i], nullptr);
		vkDestroyImage(device, m_ColorImages[i], nullptr);
		vkDestroyImageView(device, m_ColorImageViews[i], nullptr);
	}
	for (int i = 0; i != m_DepthImages.size(); ++i) {
		vkFreeMemory(device, m_DepthImageMemory[i], nullptr);
		vkDestroyImage(device, m_DepthImages[i], nullptr);
		vkDestroyImageView(device, m_DepthImageViews[i], nullptr);
	}
		

	uint32_t graphicsQfi = VulkanAPI::GetGraphicsQFI();

	// use same format as swapchaing image, maybe change that later.
	m_ColorFormat = m_Swapchain.GetColorFormat();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_Width;
	imageInfo.extent.height = m_Height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	// transition later.
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// will only be accessed by one queue at a time (for now).
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 1;
	// needs to only be available to graphics queue.
	imageInfo.pQueueFamilyIndices = &graphicsQfi;

	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;

	// Create Image View
	VkImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;


	imageInfo.format = m_ColorFormat;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	imageViewCreateInfo.format = m_ColorFormat;
	for (int i = 0; i != m_ImageCount; ++i) {
		vkCreateImage(device, &imageInfo, nullptr, &m_ColorImages[i]);

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, m_ColorImages[i], &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &m_ColorImageMemory[i]));
		ASSERT_VULKAN(vkBindImageMemory(device, m_ColorImages[i], m_ColorImageMemory[i], 0));

		imageViewCreateInfo.image = m_ColorImages[i];
		ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ColorImageViews[i]));
	}

	// create depth format.
	std::vector<VkFormat> m_DepthFormatCandidates = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT };
	m_DepthFormat = VulkanAPI::FindSupportedFormat(
		m_DepthFormatCandidates,
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	if (m_DepthFormat == VK_FORMAT_UNDEFINED) {
		Log::Error("Failed to find Depth Format", true);
		exit(1);
	}

	imageInfo.format = m_DepthFormat;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	imageViewCreateInfo.format = m_DepthFormat;
	for (int i = 0; i != m_ImageCount; ++i) {
		vkCreateImage(device, &imageInfo, nullptr, &m_DepthImages[i]);

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, m_DepthImages[i], &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memReqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &m_DepthImageMemory[i]));
		ASSERT_VULKAN(vkBindImageMemory(device, m_DepthImages[i], m_DepthImageMemory[i], 0));

		imageViewCreateInfo.image = m_DepthImages[i];
		ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_DepthImageViews[i]));
	}
}

void SubpassRenderer::CreateRenderPass() {
	VkDevice device = VulkanAPI::GetDevice();
	vkDestroyRenderPass(device, m_RenderPass, nullptr);

	VkAttachmentDescription colorAttachment;
	colorAttachment.flags = 0;
	colorAttachment.format = m_ColorFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// don't care about layout as it will be cleared (same with depth).
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentReference;
	colorAttachmentReference.attachment = COLOR_ATTACHMENT_INDEX;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment;
	depthAttachment.flags = 0;
	depthAttachment.format = m_DepthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference;
	depthAttachmentReference.attachment = DEPTH_ATTACHMENT_INDEX;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription swapchainAttachment;
	swapchainAttachment.flags = 0;
	swapchainAttachment.format = m_Swapchain.GetColorFormat();
	swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference swapchainAttachmentReference;
	swapchainAttachmentReference.attachment = SWAPCHAIN_ATTACHMENT_INDEX;
	swapchainAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::vector<VkSubpassDescription> subpassDescriptions(m_Subpasses.size());
	for (int i = 0; i != m_Subpasses.size(); ++i) {
		auto [description, contents] = m_Subpasses[i]->GetSubpass(
			i,
			    colorAttachmentReference,
			    depthAttachmentReference,
			swapchainAttachmentReference );
		// returned subpasses have to contain pointers to attachmentReferences,
		// so they are passed by reference.
		subpassDescriptions[i] = description;
		// store in member, needed when recording command buffers.
		m_SubpassContents[i] = contents;
	}

	std::vector<VkAttachmentDescription> attachments {
		colorAttachment, depthAttachment, swapchainAttachment
	};
	// Create RenderPass
	VkRenderPassCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = subpassDescriptions.size();
	createInfo.pSubpasses = subpassDescriptions.data();
	createInfo.dependencyCount = m_SubpassDependencies.size();
	createInfo.pDependencies = m_SubpassDependencies.data();

	VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, &m_RenderPass);
	ASSERT_VULKAN(result);

	for (size_t i = 0; i != m_Subpasses.size(); i++)
		m_Subpasses[i]->CreatePipeline(i, m_RenderPass);
}

void SubpassRenderer::CreateFramebuffers() {
	VkDevice device = VulkanAPI::GetDevice();
	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);

	auto swapchainImageViews = m_Swapchain.GetColorImageViews();
	std::vector<VkImageView> attachments(ATTACHMENT_COUNT);
	for (int i = 0; i != m_ImageCount; ++i) {
		attachments[COLOR_ATTACHMENT_INDEX] = m_ColorImageViews[i];
		attachments[DEPTH_ATTACHMENT_INDEX] = m_DepthImageViews[i];
		attachments[SWAPCHAIN_ATTACHMENT_INDEX] = swapchainImageViews[i];

		VkFramebufferCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.renderPass = m_RenderPass;
		createInfo.attachmentCount = ATTACHMENT_COUNT;
		createInfo.pAttachments = attachments.data();
		createInfo.width = m_Swapchain.m_Width;
		createInfo.height = m_Swapchain.m_Height;
		createInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(VulkanAPI::GetDevice(), &createInfo, nullptr, &m_Framebuffers[i]);
		ASSERT_VULKAN(result);
	}
}

void SubpassRenderer::CreateCommandBuffers() {
	// one for each framebuffer.
	m_CommandPool.AllocateBuffers(m_ImageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_CommandBuffers = m_CommandPool.GetBuffers();
}

void SubpassRenderer::RecordFrameCommandBuffer(size_t indx) {
	// Begin command buffer
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	ASSERT_VULKAN(vkBeginCommandBuffer(m_CommandBuffers[indx], &beginInfo));

	VkClearValue depthClearValue = { 1.0f, 0 };
	VkClearValue colorClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
	std::vector<VkClearValue> clearValues(ATTACHMENT_COUNT);
	clearValues[COLOR_ATTACHMENT_INDEX] = colorClearValue;
	clearValues[SWAPCHAIN_ATTACHMENT_INDEX] = colorClearValue;
	clearValues[DEPTH_ATTACHMENT_INDEX] = depthClearValue;

	VkRenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = m_RenderPass;
	renderPassBeginInfo.framebuffer = m_Framebuffers[indx];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = { m_Width, m_Height };
	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_CommandBuffers[indx], &renderPassBeginInfo, m_SubpassContents[0]);

	m_Subpasses[0]->RecordFrameCommandBuffer(m_CommandBuffers[indx], indx);

	for (int j = 1; j != m_Subpasses.size(); ++j) {
		vkCmdNextSubpass(m_CommandBuffers[indx], m_SubpassContents[j]);
		m_Subpasses[j]->RecordFrameCommandBuffer(m_CommandBuffers[indx], indx);
	}

	vkCmdEndRenderPass(m_CommandBuffers[indx]);
	vkEndCommandBuffer(m_CommandBuffers[indx]);
}

void SubpassRenderer::Frame(VkQueue queue, size_t indx, VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
	RecordFrameCommandBuffer(indx);

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = waitSemaphore != nullptr ? 1 : 0;
	submitInfo.pWaitSemaphores = waitSemaphore;
	submitInfo.pWaitDstStageMask = &waitFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CommandBuffers[indx];
	submitInfo.signalSemaphoreCount = signalSemaphore != nullptr ? 1 : 0;
	submitInfo.pSignalSemaphores = signalSemaphore;

	VkResult result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	ASSERT_VULKAN(result);
}

}
