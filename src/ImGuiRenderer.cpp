#include <engine/graphics/renderer/ImGuiRenderer.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <engine/graphics/Window.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace en
{
	ImGuiRenderer::ImGuiRenderer(size_t maxConcurrent) :
		Subpass(),
		m_MaxConcurrent{maxConcurrent} {

		VkDevice device = VulkanAPI::GetDevice();
		
		CreateImGuiDescriptorPool(device);

		// Init imgui backend
		IMGUI_CHECKVERSION();
		ImGui::SetCurrentContext(ImGui::CreateContext());
		ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWHandle(), true);

	}

	ImGuiRenderer::~ImGuiRenderer()
	{
		VkDevice device = VulkanAPI::GetDevice();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		vkDestroyDescriptorPool(device, m_ImGuiDescriptorPool, nullptr);
	}

	void ImGuiRenderer::Resize(uint32_t width, uint32_t height) {
		// ImGui_ImplVulkan_Shutdown();
	}

	void ImGuiRenderer::StartFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiRenderer::EndFrame(VkQueue queue, size_t imageIndx)
	{
		// Calculate ImGui draw data
		ImGui::Render();
		m_FrameDrawData = ImGui::GetDrawData();
	}

	void ImGuiRenderer::CreateImGuiDescriptorPool(VkDevice device)
	{
        std::vector<VkDescriptorPoolSize> descriptorPoolSizes =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.pNext = nullptr;
        descriptorPoolCreateInfo.flags = 0;
        descriptorPoolCreateInfo.maxSets = 1000;
        descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

        VkResult result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &m_ImGuiDescriptorPool);
        ASSERT_VULKAN(result);
	}

	std::pair<VkSubpassDescription, VkSubpassContents> ImGuiRenderer::GetSubpass(
		size_t subpass_indx,
		const VkAttachmentReference &colorAttachment,
		const VkAttachmentReference &depthAttachment,
		const VkAttachmentReference &swapchainAttachment) {

		return {
			VkSubpassDescription {
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = nullptr,
				.colorAttachmentCount = 1,
				.pColorAttachments = &swapchainAttachment,
				.pResolveAttachments = nullptr,
				.pDepthStencilAttachment = nullptr,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = nullptr,
			},
			VK_SUBPASS_CONTENTS_INLINE
		};

		// VkSubpassDependency subpassDependency;
		// subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		// subpassDependency.dstSubpass = 0;
		// subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// subpassDependency.srcAccessMask = 0;
		// subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		// subpassDependency.dependencyFlags = 0;
    }

	void ImGuiRenderer::RecordFrameCommandBuffer(VkCommandBuffer buf, size_t frame_indx) {
		ImGui_ImplVulkan_RenderDrawData(m_FrameDrawData, buf);
	}

	void CheckVkResult(VkResult result)
	{
		ASSERT_VULKAN(result);
	}

	void ImGuiRenderer::CreatePipeline(size_t subpass, const VkRenderPass renderPass) {
		m_RenderPass = renderPass;
		m_Subpass = subpass;

		uint32_t qfi = VulkanAPI::GetGraphicsQFI();
		VkQueue queue = VulkanAPI::GetGraphicsQueue();

		ImGui_ImplVulkan_InitInfo implVulkanInitInfo = {};
		implVulkanInitInfo.Instance = VulkanAPI::GetInstance();
		implVulkanInitInfo.PhysicalDevice = VulkanAPI::GetPhysicalDevice();
		implVulkanInitInfo.Device = VulkanAPI::GetDevice();
		implVulkanInitInfo.QueueFamily = qfi;
		implVulkanInitInfo.Queue = queue;
		implVulkanInitInfo.PipelineCache = nullptr;
		implVulkanInitInfo.DescriptorPool = m_ImGuiDescriptorPool;
		implVulkanInitInfo.Subpass = subpass;
		implVulkanInitInfo.MinImageCount = m_MaxConcurrent;
		implVulkanInitInfo.ImageCount = m_MaxConcurrent;
		implVulkanInitInfo.Allocator = nullptr;
		implVulkanInitInfo.CheckVkResultFn = CheckVkResult;
		if (!ImGui_ImplVulkan_Init(&implVulkanInitInfo, renderPass))
			Log::Error("Failed to Init ImGuiManager", true);

		// Init imgui on GPU
		vk::CommandPool commandPool(0, qfi);
		commandPool.AllocateBuffers(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VkCommandBuffer commandBuffer = commandPool.GetBuffer(0);

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		ASSERT_VULKAN(result);

		ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

		result = vkEndCommandBuffer(commandBuffer);
		ASSERT_VULKAN(result);

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		ASSERT_VULKAN(result);

		result = vkQueueWaitIdle(queue);
		ASSERT_VULKAN(result);

		commandPool.Destroy();
	}
}
