#pragma once

#include <engine/graphics/Common.hpp>
#include <functional>
#include <vector>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <vulkan/vulkan_core.h>

namespace en::vk
{
	class Swapchain
	{
	public:
		Swapchain(
			uint32_t width,
			uint32_t height);

		void Destroy(bool destroySwapchain);

		void Resize();
		void DrawAndPresent();

		uint32_t GetImageCount() const;
		int AcquireImage();
		int AcquireImages();
		uint32_t WaitForImage();
		void Submit(uint32_t indx, VkSemaphore *waitSemaphore);
		VkFormat GetColorFormat() const;
		VkFormat GetDepthFormat() const;
		VkImage GetDepthImage() const;
		VkImageView GetDepthImageView() const;
		VkDescriptorSetLayout GetSamplerLayout() const;
		VkDescriptorSet GetDepthSampler() const;
		VkDescriptorSet GetColorSampler(size_t indx) const;
		void SetResizeCallback(std::function<void(uint32_t,uint32_t)>);
		std::vector<VkImage> GetColorImages() const;
		std::vector<VkImageView> GetColorImageViews() const;

		VkSwapchainKHR GetHandle() const;
		uint32_t m_Width;
		uint32_t m_Height;

	private:
		std::vector<VkFence> m_ImageAvailableFences;
		std::vector<uint32_t> m_ImageIndices;

		VkSwapchainKHR m_Handle;

		VkDescriptorPool m_DescriptorPool;
		VkSampler m_Sampler;
		VkDescriptorSetLayout m_SampleDescriptorLayout;

		VkFormat m_DepthFormat;
		VkImage m_DepthImage;
		VkDeviceMemory m_DepthImageMemory;
		VkImageView m_DepthImageView;
		VkDescriptorSet m_DepthSampleDescriptor;

		std::function<void(uint32_t, uint32_t)> m_ResizeCallback;

		uint32_t m_ImageCount;
		std::vector<VkImage> m_ColorImages;
		std::vector<VkImageView> m_ColorImageViews;
		std::vector<VkDescriptorSet> m_ColorImageSampleDescriptors;
		VkFormat m_ColorFormat;

		void (*m_RecordCommandBufferFunc)(VkCommandBuffer, VkImage);

		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_RenderFinishedSemaphore;

		void CreateSwapchain(VkDevice device, VkSurfaceFormatKHR surfaceFormat, uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain);
		void RetreiveImages(VkDevice device);
		void CreateSyncObjects(VkDevice device);
		void CreateColorImageObjects(VkDevice device);

		void CreateDepthBuffer(VkDevice device);
		void CreateSampleDescriptorObjects(VkDevice device);
		void UpdateDescriptorSets(VkDevice device);
	};
}
