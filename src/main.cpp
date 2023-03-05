// for cmath
#include "engine/graphics/AerialPerspective.hpp"
#include "engine/graphics/Atmosphere.hpp"
#include "engine/graphics/GroundLighting.hpp"
#include "engine/graphics/renderer/AerialPerspectiveRenderer.hpp"
#include "engine/graphics/renderer/PostprocessingSubpass.hpp"
#define _USE_MATH_DEFINES

#include "engine/graphics/renderer/SkyRenderer.hpp"
#include <engine/graphics/VulkanAPI.hpp>
#include <engine/util/Log.hpp>
#include <engine/graphics/Window.hpp>
#include <engine/graphics/vulkan/Swapchain.hpp>
#include <engine/graphics/vulkan/CommandRecorder.hpp>
#include <engine/graphics/Camera.hpp>
#include <engine/graphics/Sun.hpp>
#include <engine/graphics/renderer/SimpleModelRenderer.hpp>
#include <glm/gtx/transform.hpp>
#include <engine/graphics/renderer/ImGuiRenderer.hpp>
#include <imgui.h>
#include <vulkan/vulkan_core.h>
#include <cmath>
#include <engine/graphics/EnvConditions.hpp>
#include <engine/graphics/Precomputer.hpp>
#include <engine/graphics/renderer/SubpassRenderer.hpp>
#include <engine/graphics/Subpass.hpp>
#include <engine/graphics/renderer/CloudRenderer.hpp>
#include <engine/util/Input.hpp>
#include <engine/util/Time.hpp>
#include <engine/objects/CloudData.hpp>
#include <engine/objects/Terrain.hpp>
#include <engine/objects/Wind.hpp>

en::EnvConditions::Environment earthConditions {
	// data from https://www.iup.uni-bremen.de/gruppen/molspec/downloads/serdyuchenkogorshelev5digits.dat.
	// 650, 510, 475nm at 243K (-30C, eyeballed average temperature for height of most ozone).
	// *10^-4 for cm^2 -> m^2.
	.m_OzoneExtinctionCoefficient = glm::vec3(2.43181E-25, 1.151113e-25, 4.47939e-26),
	.m_PlanetRadius = 6371000,
	.m_AtmosphereHeight = 80000,
	.m_RefractiveIndexAir = 1.0003,
	.m_AirDensityAtSeaLevel = static_cast<float>(2.545*pow(10, 25)),
	.m_MieScatteringCoefficient = 0.000002,
	.m_AsymmetryFactor = 0.73,
	.m_RayleighScaleHeight = 8000,
	.m_MieScaleHeight = 1200
};

en::CloudRenderer* cloudRenderer;

std::vector<VkSemaphore> precomp_aerial_semaphores;
std::vector<VkSemaphore> aerial_render_semaphores;
std::vector<VkSemaphore> render_submit_semaphores;

void CreateRenderSemaphores(size_t num) {
	precomp_aerial_semaphores.resize(num);
	aerial_render_semaphores.resize(num);
	render_submit_semaphores.resize(num);

	VkSemaphoreCreateInfo sem_inf {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0
	};

	for (int i = 0; i != num; ++i) {
		vkCreateSemaphore(en::VulkanAPI::GetDevice(), &sem_inf, nullptr, &precomp_aerial_semaphores[i]);
		vkCreateSemaphore(en::VulkanAPI::GetDevice(), &sem_inf, nullptr, &aerial_render_semaphores[i]);
		vkCreateSemaphore(en::VulkanAPI::GetDevice(), &sem_inf, nullptr, &render_submit_semaphores[i]);
	}
	// if (cloudRenderer != nullptr && en::ImGuiRenderer::IsInitialized())
	// {
	// 	VkImageCopy imageCopy;
	// 	imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	// 	imageCopy.srcSubresource.mipLevel = 0;
	// 	imageCopy.srcSubresource.baseArrayLayer = 0;
	// 	imageCopy.srcSubresource.layerCount = 1;
	// 	imageCopy.srcOffset = { 0, 0, 0 };
	// 	imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	// 	imageCopy.dstSubresource.mipLevel = 0;
	// 	imageCopy.dstSubresource.baseArrayLayer = 0;
	// 	imageCopy.dstSubresource.layerCount = 1;
	// 	imageCopy.dstOffset = { 0, 0, 0 };
	// 	imageCopy.extent = { width, height, 1 };

	// 	vkCmdCopyImage(
	// 		commandBuffer,
	// 		en::ImGuiRenderer::GetImage(),
	// 		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// 		image,
	// 		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	// 		1,
	// 		&imageCopy);
	// }
}

void DestroyRenderSemaphores() {
	for (int i = 0; i != precomp_aerial_semaphores.size(); ++i) {
		vkDestroySemaphore(en::VulkanAPI::GetDevice(), precomp_aerial_semaphores[i], nullptr);
		vkDestroySemaphore(en::VulkanAPI::GetDevice(), aerial_render_semaphores[i], nullptr);
		vkDestroySemaphore(en::VulkanAPI::GetDevice(), render_submit_semaphores[i], nullptr);
	}
}

void SwapchainResizeCallback()
{
	en::Window::WaitForUsableSize();
	vkDeviceWaitIdle(en::VulkanAPI::GetDevice());

	uint32_t width = en::Window::GetWidth();
	uint32_t height = en::Window::GetHeight();
	cloudRenderer->Resize(width, height);
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
	en::vk::Swapchain swapchain(width, height);
	en::Camera camera(
		glm::vec3(0.0f, 0.5f, -5.0f),
		M_PI_2,
		glm::vec3(0.0f, 1.0f, 0.0f),
		width, height,
		glm::radians(60.0f),
		1.0f,
		100000.0f);

	en::Sun::Init();
	en::Sun sun(M_PI_2, 0, glm::vec3(10));

	en::Wind wind(M_PI / 8.0f, 0.0f);

	// TODO: clean up all of this, precomp should own atmosphere and env.
	en::EnvConditions earthEnv(earthConditions);
	en::Atmosphere atmosphere(earthEnv.GetDescriptorSetLayout());
	//en::DirLight dirLight(glm::vec3(0.4f, -0.2f, -0.4f));

	//modelRenderer = new en::SimpleModelRenderer(width, height, &camera);
	en::Precomputer precomp(atmosphere, earthEnv, 1, 1, 1);

	en::CloudData cloudData;
	auto cloudRenderer = std::make_shared<en::CloudRenderer>(width, height, &camera, &sun, &wind, &cloudData, &atmosphere, &precomp);

	en::GroundLighting gl(precomp, atmosphere, sun);

	en::AerialPerspective aerial(camera, precomp, atmosphere, sun);

	auto modelRenderer = std::make_shared<en::SimpleModelRenderer>(width, height, &camera, &sun, gl, swapchain.GetImageCount());
	auto postProcess = std::make_shared<en::PostprocessingSubpass>(width, height);

	auto skyRenderer = std::make_shared<en::SkyRenderer>(
		swapchain.GetImageCount(), 
		width,
		height,
		&camera,
		sun,
		swapchain,
		atmosphere,
		precomp,
		aerial );

	auto aerialPerspectiveRenderer = std::make_shared<en::AerialPerspectiveRenderer>(
		swapchain.GetImageCount(), 
		width,
		height,
		&camera,
		sun,
		swapchain,
		atmosphere,
		precomp,
		aerial );

	auto imguiRenderer = std::make_shared<en::ImGuiRenderer>(swapchain.GetImageCount());

	en::SubpassRenderer spr = en::SubpassRenderer(std::vector<std::shared_ptr<en::Subpass>>({
		modelRenderer,
		skyRenderer,
		cloudRenderer,
		aerialPerspectiveRenderer,
		postProcess,
		imguiRenderer
	}), std::vector<VkSubpassDependency>({
		{
			// AerialPerspectiveRenderer (subpass 3) needs to wait for aerialPerspective-
			// data (calculated in compute shader, consumed in fragment shader).
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 3,
			.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = 0
		},
		{
			// from image acquire to model.
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			// only wait with write, won't need to read in first subpass.
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0
		},
		{
			// simply sync color-output for now.
			.srcSubpass = 0,
			.dstSubpass = 1,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			.dependencyFlags = 0
		},
		{
			// simply sync color-output for now.
			.srcSubpass = 1,
			.dstSubpass = 2,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			.dependencyFlags = 0
		},
		{
			// simply sync color-output for now.
			.srcSubpass = 2,
			.dstSubpass = 3,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
			.dependencyFlags = 0
		},
		{
			.srcSubpass = 3,
			.dstSubpass = 4,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.dependencyFlags = 0
		},
		{
			.srcSubpass = 4,
			.dstSubpass = 5,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			.dependencyFlags = 0
		},
		}),
	swapchain);

	swapchain.SetResizeCallback(
		[
			&camera,
			&modelRenderer,
			&skyRenderer,
			&imguiRenderer,
			&aerialPerspectiveRenderer,
			&spr,
			&postProcess,
			&cloudRenderer
		]
		(uint32_t width, uint32_t height) {
		vkDeviceWaitIdle(en::VulkanAPI::GetDevice());

		camera.SetAspectRatio(width, height);
		camera.UpdateUBO();

		skyRenderer->ResizeFrame(width, height);
		aerialPerspectiveRenderer->ResizeFrame(width, height);
		imguiRenderer->Resize(width, height);
		modelRenderer->ResizeFrame(width, height);
		postProcess->Resize(width, height);
		cloudRenderer->Resize(width, height);

		spr.Resize(width, height);

		modelRenderer->RecordCommandBuffers();
	});
	
	// Model
	en::Terrain terrain(400, 20.0f, 0.0625f, 128.0f, 3.5f, 0.0f, glm::vec2(0.0f));
	en::ModelInstance terrainInstance(reinterpret_cast<en::Model*>(&terrain), glm::mat4(1.0f));
	modelRenderer->AddModelInstance(&terrainInstance);

	en::Terrain bgTerrain(400, 128.0f, 0.0625f, 1024.0f, 2.0f, 32.0f, glm::vec2(20.0f));
	en::ModelInstance bgTerrainInstance(reinterpret_cast<en::Model*>(&bgTerrain), glm::translate(glm::vec3(0.0f, -128.0f, 0.0f)));
	modelRenderer->AddModelInstance(&bgTerrainInstance);

	en::Model dragonModel("dragon.obj", false);
	en::ModelInstance dragonInstance(&dragonModel, glm::mat4(1.0f));
	modelRenderer->AddModelInstance(&dragonInstance);

	en::Model backpackModel("backpack/backpack.obj", false);
	en::ModelInstance backpackInstance(&backpackModel, glm::mat4(1.0f));
	// modelRenderer.AddModelInstance(&backpackInstance);

	// Main loop
	VkDevice device = en::VulkanAPI::GetDevice();
	VkQueue graphicsQueue = en::VulkanAPI::GetGraphicsQueue();
	VkResult result;
	float t = 0.0f;
	float dragon_dist = 10;
	float dragon_scale = 10;

	CreateRenderSemaphores(swapchain.GetImageCount());
	// swapchain.AcquireImages();
	while (!en::Window::IsClosed())
	{
		en::Window::Update();
		if (swapchain.AcquireImage() == -1) {
			// resized:
			// the swapchain was recreated, but acquired images can still be presented.
			// Wait for them here.
			// Re-acquire images.
			// swapchain.AcquireImages();
			continue;
		}
		uint32_t imageIndx = swapchain.WaitForImage();
		// swapchain.AcquireImage(imageIndx);
		// swapchain.AcquireImage();

		// Update
		width = en::Window::GetWidth();
		height = en::Window::GetHeight();

		t += 0.01f;

		imguiRenderer->StartFrame();

		cloudData.RenderImGui();
		//dirLight.RenderImGui();
		camera.RenderImgui();
		earthEnv.RenderImgui();
		sun.RenderImgui();
		ImGui::DragFloat("dragon_z", &dragon_dist, 100000, -1000000, 10000000000, "%g", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("dragon_scale", &dragon_scale, 100000, 0, 10000000000, "%g", ImGuiSliderFlags_Logarithmic);
		precomp.RenderImgui();

		imguiRenderer->EndFrame(graphicsQueue, imageIndx);

		en::Input::Update();
		en::Time::Update();
		float deltaTime = static_cast<float>(en::Time::GetDeltaTime());
		en::Input::HandleUserCamInput(&camera, deltaTime);
		wind.Update(deltaTime);
		// sun.SetZenith(fmod(sun.GetZenith() + deltaTime * -0.1f, 2 * M_PI)); // Changing daytime
		camera.UpdateUBO();

		camera.SetAspectRatio(width, height);
		// TODO: camera.UpdateUniformBuffer();

		dragonInstance.SetModelMat(
			glm::translate(glm::vec3(0.0f, -1.0f, dragon_dist)) *
			glm::rotate(glm::radians(t), glm::vec3(0.0f, 1.0f, 0.0f)) *
			glm::scale(glm::vec3(dragon_scale)));
		// backpackInstance.SetModelMat(
		// 	glm::translate(glm::vec3(2.0f, 0.0f, 0.0f)) *
		// 	glm::rotate(glm::radians(t), glm::vec3(0.0f, 1.0f, 0.0f)) *
		// 	glm::scale(glm::vec3(1)));

		// don't wait for any semphores.
		precomp.Frame();
		gl.Compute();

		// no need to wait for precomputer, it either
		// * renders into an completely offscreen image, which will only be accessed in later frames.
		// * increases the ratio, eg. uploads data to the GPU.
		aerial.Compute(nullptr, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr);

		// wait with fragment shader-evaluation, we'll need new aerial-precomputation.
		// TODO: wait in specific subpass only??
		spr.Frame(graphicsQueue, imageIndx, nullptr, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &render_submit_semaphores[imageIndx]);

		swapchain.Submit(imageIndx, &render_submit_semaphores[imageIndx]);
		// cannot handle multiple concurrent frames yet.
		vkDeviceWaitIdle(device);
	}
	result = vkDeviceWaitIdle(device);
	ASSERT_VULKAN(result);

	DestroyRenderSemaphores();

	// Destroy models
	dragonInstance.Destroy();
	dragonModel.Destroy();

	backpackInstance.Destroy();
	backpackModel.Destroy();

	bgTerrainInstance.Destroy();
	reinterpret_cast<en::Model*>(&bgTerrain)->Destroy();

	terrainInstance.Destroy();
	reinterpret_cast<en::Model*>(&terrain)->Destroy();

	// Destroy graphics resources
	(*imguiRenderer).~ImGuiRenderer();

	swapchain.Destroy(true);

	(*skyRenderer).Destroy();
	(*aerialPerspectiveRenderer).Destroy();
	modelRenderer->Destroy();
	cloudRenderer->Destroy();

	cloudData.Destroy();

	//dirLight.Destroy();

	wind.Destroy();

	camera.Destroy();

	earthEnv.~EnvConditions();
	sun.~Sun();
	en::Sun::Destroy();
	atmosphere.~Atmosphere();
	precomp.~Precomputer();
	gl.~GroundLighting();
	aerial.~AerialPerspective();

	(*postProcess).~PostprocessingSubpass();
	spr.~SubpassRenderer();

	// Destroy engine
    en::VulkanAPI::Shutdown();
    en::Window::Shutdown();

    en::Log::Info("Ending SkyRenderer");

	// exit() to prevent calling sun's destructor a second time.
	exit(0);
}
