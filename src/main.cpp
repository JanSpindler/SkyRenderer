#include <engine/graphics/VulkanAPI.hpp>
#include <engine/util/Log.hpp>
#include <engine/graphics/Window.hpp>
#include <engine/graphics/vulkan/Swapchain.hpp>
#include <engine/graphics/vulkan/CommandRecorder.hpp>
#include <engine/graphics/Camera.hpp>
#include <engine/graphics/renderer/SimpleModelRenderer.hpp>
#include <glm/gtx/transform.hpp>
#include <engine/graphics/renderer/ImGuiRenderer.hpp>
#include <imgui.h>
#include <engine/graphics/renderer/CloudRenderer.hpp>
#include <engine/util/Input.hpp>
#include <engine/util/Time.hpp>
#include <engine/objects/CloudData.hpp>
#include <engine/graphics/Sun.hpp>

en::CloudRenderer* cloudRenderer;

void RecordSwapchainCommandBuffer(VkCommandBuffer commandBuffer, VkImage image)
{
	uint32_t width = en::Window::GetWidth();
	uint32_t height = en::Window::GetHeight();

	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
		en::Log::Error("Failed to begin VkCommandBuffer", true);
	
	en::vk::CommandRecorder::ImageLayoutTransfer(
		commandBuffer,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_ACCESS_NONE_KHR,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);

	if (cloudRenderer != nullptr && en::ImGuiRenderer::IsInitialized())
	{
		VkImageCopy imageCopy;
		imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.srcSubresource.mipLevel = 0;
		imageCopy.srcSubresource.baseArrayLayer = 0;
		imageCopy.srcSubresource.layerCount = 1;
		imageCopy.srcOffset = { 0, 0, 0 };
		imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.dstSubresource.mipLevel = 0;
		imageCopy.dstSubresource.baseArrayLayer = 0;
		imageCopy.dstSubresource.layerCount = 1;
		imageCopy.dstOffset = { 0, 0, 0 };
		imageCopy.extent = { width, height, 1 };

		vkCmdCopyImage(
			commandBuffer,
			en::ImGuiRenderer::GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopy);
	}

	en::vk::CommandRecorder::ImageLayoutTransfer(
		commandBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS)
		en::Log::Error("Failed to end VkCommandBuffer", true);
}

void SwapchainResizeCallback()
{
	en::Window::WaitForUsableSize();
	vkDeviceWaitIdle(en::VulkanAPI::GetDevice());

	uint32_t width = en::Window::GetWidth();
	uint32_t height = en::Window::GetHeight();

	en::Log::Info(std::to_string(width) + " | " + std::to_string(height));

	cloudRenderer->Resize(width, height);
	en::ImGuiRenderer::Resize(width, height);
	en::ImGuiRenderer::SetBackgroundImageView(cloudRenderer->GetImageView());
}

int main(int argc, char** argv)
{
    en::Log::Info("Starting SkyRenderer");

	// Engine
    en::Window::Init(800, 600, "SkyRenderer");
    en::VulkanAPI::Init("SkyRenderer");
	en::Input::Init(en::Window::GetGLFWHandle());

    uint32_t width = en::Window::GetWidth();
    uint32_t height = en::Window::GetHeight();

	// Graphics
	en::vk::Swapchain swapchain(width, height, RecordSwapchainCommandBuffer, SwapchainResizeCallback);
	
	en::Camera camera(
		glm::vec3(0.0f, 0.0f, -5.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		static_cast<float>(width) / static_cast<float>(height),
		glm::radians(60.0f),
		0.1f,
		100.0f);

	en::Sun sun(-1.9f, -0.5f, glm::vec3(1.0f));

	en::CloudData cloudData;

	cloudRenderer = new en::CloudRenderer(width, height, &camera, &sun, &cloudData);

	en::ImGuiRenderer::Init(width, height);
	en::ImGuiRenderer::SetBackgroundImageView(cloudRenderer->GetImageView());

	swapchain.Resize(width, height); // For rerecording the command buffers after renderer creation

	// Main loop
	VkDevice device = en::VulkanAPI::GetDevice();
	VkQueue graphicsQueue = en::VulkanAPI::GetGraphicsQueue();
	VkResult result;
	float t = 0.0f;
	while (!en::Window::IsClosed())
	{
		// Update
		en::Window::Update();
		en::Input::Update();
		en::Time::Update();
		width = en::Window::GetWidth();
		height = en::Window::GetHeight();
		float deltaTime = static_cast<float>(en::Time::GetDeltaTime());
		en::Input::HandleUserCamInput(&camera, deltaTime);

		t += 0.01f;

		// Physics
		camera.SetAspectRatio(width, height);
		camera.UpdateUniformBuffer();

		// Render
		en::ImGuiRenderer::StartFrame();

		std::chrono::time_point<std::chrono::high_resolution_clock> pre = std::chrono::high_resolution_clock::now();

		cloudRenderer->Render(graphicsQueue);
		result = vkQueueWaitIdle(graphicsQueue);
		ASSERT_VULKAN(result);
		
		std::chrono::time_point<std::chrono::high_resolution_clock> post = std::chrono::high_resolution_clock::now();
		std::chrono::nanoseconds delta = post - pre;
		double cloudFrameTime = (double)delta.count() / 1000000.0; // in [ms] not [s]
		en::Log::Info(std::to_string(cloudFrameTime));

		cloudData.RenderImGui();
		sun.RenderImgui();
		en::ImGuiRenderer::EndFrame(graphicsQueue);
		result = vkQueueWaitIdle(graphicsQueue);
		ASSERT_VULKAN(result);

		swapchain.DrawAndPresent();
	}
	result = vkDeviceWaitIdle(device);
	ASSERT_VULKAN(result);

	// Destroy graphics resources
	en::ImGuiRenderer::Shutdown();

	swapchain.Destroy(true);

	cloudRenderer->Destroy();
	delete cloudRenderer;

	cloudData.Destroy();

	sun.Destroy();

	camera.Destroy();

	// Destroy engine
    en::VulkanAPI::Shutdown();
    en::Window::Shutdown();

    en::Log::Info("Ending SkyRenderer");
    return 0;
}
