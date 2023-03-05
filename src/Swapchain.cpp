#include <algorithm>
#include <cstdint>
#include <engine/graphics/vulkan/Swapchain.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <engine/graphics/Window.hpp>
#include <engine/util/Log.hpp>
#include <vulkan/vulkan_core.h>

namespace en::vk
{
	Swapchain::Swapchain(
		uint32_t width,
		uint32_t height)
		:
		m_Width(width),
		m_Height(height)
	{
		VkDevice device = VulkanAPI::GetDevice();
		m_ImageCount = VulkanAPI::GetSwapchainImageCount();
		VkSurfaceFormatKHR surfaceFormat = VulkanAPI::GetSurfaceFormat();

		CreateSwapchain(device, surfaceFormat, width, height, VK_NULL_HANDLE);
		CreateDepthBuffer(device);
		RetreiveImages(device);
		CreateColorImageObjects(device);
		CreateSyncObjects(device);
		CreateSampleDescriptorObjects(device);
		UpdateDescriptorSets(device);
		m_ImageIndices.resize(m_ImageCount);
	}

	void Swapchain::Destroy(bool destroySwapchain)
	{
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroySemaphore(device, m_ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, m_RenderFinishedSemaphore, nullptr);

		if (destroySwapchain) {
			vkDestroySwapchainKHR(device, m_Handle, nullptr);
			vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
			vkDestroySampler(device, m_Sampler, nullptr);
			vkDestroyDescriptorSetLayout(device, m_SampleDescriptorLayout, nullptr);
		}

		m_ColorImages.clear();

		vkFreeMemory(device, m_DepthImageMemory, nullptr);
		vkDestroyImage(device, m_DepthImage, nullptr);
		vkDestroyImageView(device, m_DepthImageView, nullptr);
		for (size_t i = 0; i != m_ColorImageViews.size(); ++i)
			vkDestroyImageView(device, m_ColorImageViews[i], nullptr);
		for (int i = 0; i != m_ImageAvailableFences.size(); ++i)
			vkDestroyFence(device, m_ImageAvailableFences[i], nullptr);
		m_ColorImageViews.clear();
	}

	void Swapchain::Resize()
	{
		Destroy(false);

		VkSwapchainKHR oldSwapchain = m_Handle;

		m_Width = Window::GetWidth();
		m_Height = Window::GetHeight();

		VkDevice device = VulkanAPI::GetDevice();
		m_ImageCount = VulkanAPI::GetSwapchainImageCount();
		VkSurfaceFormatKHR surfaceFormat = VulkanAPI::GetSurfaceFormat();

		CreateSwapchain(device, surfaceFormat, m_Width, m_Height, oldSwapchain); // TODO: reuse old swapchain
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

		CreateDepthBuffer(device);
		RetreiveImages(device);
		CreateColorImageObjects(device);
		CreateSyncObjects(device);

		UpdateDescriptorSets(device);
		m_ResizeCallback(m_Width, m_Height);
	}

	void Swapchain::SetResizeCallback(std::function<void(uint32_t,uint32_t)> newResizeFunc) {
		m_ResizeCallback = newResizeFunc; 
	}

	int Swapchain::AcquireImage()
	{
		VkDevice device = VulkanAPI::GetDevice();
		VkQueue graphicsQueue = VulkanAPI::GetGraphicsQueue();
		VkQueue presentQueue = VulkanAPI::GetPresentQueue();

		uint32_t newWidth = Window::GetWidth();
		uint32_t newHeight = Window::GetHeight();
		bool resized = false;
		if (newWidth != m_Width || newHeight != m_Height)
			resized = true;

		// find any currently unused image.
		int i = 0;
		for (; i != m_ImageAvailableFences.size(); ++i)
			if (vkGetFenceStatus(device, m_ImageAvailableFences[i]) == VK_NOT_READY) {
				vkResetFences(device, 1, &m_ImageAvailableFences[i]);
				break;
			}

		// Aquire image from swapchain
		VkResult result = vkAcquireNextImageKHR(
			device,
			m_Handle,
			0,
			VK_NULL_HANDLE,
			m_ImageAvailableFences[i],
			&m_ImageIndices[i] );

		if (resized || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			// wait for used fence.
			vkWaitForFences(device, 1, &m_ImageAvailableFences[i], VK_TRUE, UINT64_MAX);
			Resize();
			return -1;
		}
		else if (result != VK_SUCCESS)
		{
			Log::Error("Failed to aquire next swapchain VkImage", true);
		}

		return 0;
	}

	int Swapchain::AcquireImages()
	{
		VkDevice device = VulkanAPI::GetDevice();
		VkQueue graphicsQueue = VulkanAPI::GetGraphicsQueue();
		VkQueue presentQueue = VulkanAPI::GetPresentQueue();

		uint32_t newWidth = Window::GetWidth();
		uint32_t newHeight = Window::GetHeight();
		bool resized = false;
		if (newWidth != m_Width || newHeight != m_Height)
			resized = true;

		// Aquire image from swapchain
		for (int i = 0; i != 2; ++i) {
			vkResetFences(device, 1, &m_ImageAvailableFences[i]);

			VkResult result = vkAcquireNextImageKHR(
				device,
				m_Handle,
				UINT64_MAX,
				VK_NULL_HANDLE,
				m_ImageAvailableFences[i],
				&m_ImageIndices[i] );
		}

		return 0;
	}

	// returns indx of available image.
	uint32_t Swapchain::WaitForImage() {
		VkDevice device = VulkanAPI::GetDevice();
		// wait for a single image, maximum timeout.
		vkWaitForFences(device, m_ImageAvailableFences.size(), m_ImageAvailableFences.data(), VK_FALSE, UINT64_MAX);
		for (int i = 0; i != m_ImageAvailableFences.size(); ++i)
			if (vkGetFenceStatus(device, m_ImageAvailableFences[i]) == VK_SUCCESS) {
				vkResetFences(device, 1, &m_ImageAvailableFences[i]);
				return m_ImageIndices[i];
			}
		Log::Error("WaitForFences returned, but none was signaled", true);
		// won't be reached.
		return -1;
	}

	void Swapchain::Submit(uint32_t indx, VkSemaphore *waitSemaphore)
	{
		VkQueue graphicsQueue = VulkanAPI::GetGraphicsQueue();
		VkQueue presentQueue = VulkanAPI::GetPresentQueue();

		// Presentation
		VkPresentInfoKHR presentInfo;
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = waitSemaphore != nullptr ? 1 : 0;
		presentInfo.pWaitSemaphores = waitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Handle;
		presentInfo.pImageIndices = &indx;
		presentInfo.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
	}

	VkSwapchainKHR Swapchain::GetHandle() const
	{
		return m_Handle;
	} 

	uint32_t Swapchain::GetImageCount() const
	{
		return m_ImageCount;
	}

	VkFormat Swapchain::GetColorFormat() const
	{
		return m_ColorFormat;
	}

	VkFormat Swapchain::GetDepthFormat() const
	{
		return m_DepthFormat;
	}

	std::vector<VkImage> Swapchain::GetColorImages() const
	{
		return m_ColorImages;
	}

	std::vector<VkImageView> Swapchain::GetColorImageViews() const
	{
		return m_ColorImageViews;
	}

	VkImage Swapchain::GetDepthImage() const
	{
		return m_DepthImage;
	}

	VkImageView Swapchain::GetDepthImageView() const {
		return m_DepthImageView;
	}

	void Swapchain::CreateSwapchain(VkDevice device, VkSurfaceFormatKHR surfaceFormat, uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain)
	{
		uint32_t graphicsQFI = VulkanAPI::GetGraphicsQFI();
		uint32_t presentQFI = VulkanAPI::GetPresentQFI();

		m_ColorFormat = surfaceFormat.format;

		VkSwapchainCreateInfoKHR createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.surface = VulkanAPI::GetSurface();
		createInfo.minImageCount = m_ImageCount;
		createInfo.imageFormat = m_ColorFormat;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = { width, height };
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		if (graphicsQFI == presentQFI)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
		else
		{
			std::vector<uint32_t> queueFamilyIndices = { graphicsQFI, presentQFI };
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
			createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
		}
		createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = VulkanAPI::GetPresentMode();
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapchain;

		VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Handle);
		ASSERT_VULKAN(result);
	}

	void Swapchain::CreateColorImageObjects(VkDevice device)
	{
		m_ColorImageViews.resize(m_ColorImages.size());

		for (size_t i = 0; i != m_ColorImages.size(); ++i)
		{
			// Create Image View
			VkImageViewCreateInfo imageViewCreateInfo;
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.pNext = nullptr;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.image = m_ColorImages[i];
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = m_ColorFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ColorImageViews[i]);
			ASSERT_VULKAN(result);
		}
	}

	void Swapchain::RetreiveImages(VkDevice device)
	{
		vkGetSwapchainImagesKHR(device, m_Handle, &m_ImageCount, nullptr);
		m_ColorImages.resize(m_ImageCount);
		vkGetSwapchainImagesKHR(device, m_Handle, &m_ImageCount, m_ColorImages.data());
	}

	void Swapchain::CreateSyncObjects(VkDevice device)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		VkResult result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphore);
		ASSERT_VULKAN(result);

		result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphore);
		ASSERT_VULKAN(result);

		VkFenceCreateInfo fenceCreateInfo;
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;
		fenceCreateInfo.flags = 0;

		m_ImageAvailableFences.resize(m_ImageCount);
		for (int i = 0; i != m_ImageCount; ++i) {
			result = vkCreateFence(device, &fenceCreateInfo, nullptr, &m_ImageAvailableFences[i]);
			ASSERT_VULKAN(result);
		}
	}

	void Swapchain::CreateDepthBuffer(VkDevice device) {
		// create depth format.
		std::vector<VkFormat> depthFormatCandidates = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT };
		m_DepthFormat = VulkanAPI::FindSupportedFormat(
			depthFormatCandidates,
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		if (m_DepthFormat == VK_FORMAT_UNDEFINED)
			Log::Error("Failed to find Depth Format", true);

		// Create Image
		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_DepthFormat;
		imageCreateInfo.extent = { m_Width, m_Height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(device, &imageCreateInfo, nullptr, &m_DepthImage);
		ASSERT_VULKAN(result);

		// Allocate Image Memory
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, m_DepthImage, &memoryRequirements);

		VkMemoryAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
			memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		result = vkAllocateMemory(device, &allocateInfo, nullptr, &m_DepthImageMemory);
		ASSERT_VULKAN(result);

		result = vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0);
		ASSERT_VULKAN(result);

		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_DepthImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_DepthFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		ASSERT_VULKAN(vkCreateImageView(
			device,
			&imageViewCreateInfo,
			nullptr,
			&m_DepthImageView));
	}

	void Swapchain::CreateSampleDescriptorObjects(VkDevice device) {
		VkDescriptorPoolSize poolSize {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

			// One for depth, one for each swapchain-image.
			//
			// TODO: strictly, the pool also needs to be recreated on swapchain-
			// recreation, but it doesn't seem like a recreation could lead to
			// imageCount being different.
			.descriptorCount = 1 + m_ImageCount
		};

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = poolSize.descriptorCount;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		ASSERT_VULKAN(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));

		VkDescriptorSetLayoutBinding sampleBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			// Sample only in fragment.
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.pBindings = &sampleBinding;

		ASSERT_VULKAN(vkCreateDescriptorSetLayout(device,
			&layoutCreateInfo,
			nullptr,
			&m_SampleDescriptorLayout));

		// Vulkan needs an array of layouts :/
		std::vector<VkDescriptorSetLayout> layouts(1+m_ImageCount);
		std::fill_n(layouts.begin(), 1+m_ImageCount, m_SampleDescriptorLayout);

		VkDescriptorSetAllocateInfo allocInfo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = 1 + m_ImageCount,
			.pSetLayouts = layouts.data()
		};

		std::vector<VkDescriptorSet> sets(m_ImageCount+1);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, sets.data()));

		m_DepthSampleDescriptor = sets[0];

		size_t indx = 1;
		m_ColorImageSampleDescriptors.resize(m_ImageCount);
		std::generate_n(m_ColorImageSampleDescriptors.begin(), m_ImageCount, [&sets, &indx](){
			// postincrement.
			return sets[indx++];
		});

		// Create sampler
		VkSamplerCreateInfo sampler;
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.pNext = nullptr;
		// will always be adressed absolutely, so these shouldn't matter, but
		// go with nearest to be safe.
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.mipLodBias = 0.0f;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareEnable = VK_FALSE;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		// Makes it easier to address in fragment shader; glFragCoords gives
		// absolute coordinates.
		sampler.unnormalizedCoordinates = VK_TRUE;
		sampler.flags = 0;
		ASSERT_VULKAN(vkCreateSampler(device, &sampler, nullptr, &m_Sampler));
	}

	void Swapchain::UpdateDescriptorSets(VkDevice device) {
		std::vector<VkImageView> views(1);
		views[0] = m_DepthImageView;
		views.insert(views.end(), m_ColorImageViews.begin(), m_ColorImageViews.end());

		std::vector<VkDescriptorImageInfo> infos(1+m_ImageCount);
		std::fill_n(infos.begin(), 1+m_ImageCount, VkDescriptorImageInfo{
			.sampler = m_Sampler,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		});

		std::vector<VkDescriptorSet> sets(1);
		sets[0] = m_DepthSampleDescriptor;
		sets.insert(sets.end(), m_ColorImageSampleDescriptors.begin(), m_ColorImageSampleDescriptors.end());


		std::vector<VkWriteDescriptorSet> writeDescSets(1+m_ImageCount);
		std::fill_n(writeDescSets.begin(), 1+m_ImageCount, VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_DepthSampleDescriptor,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		});

		for (int i = 0; i != 1 + m_ImageCount; ++i) {
			infos[i].imageView = views[i];

			writeDescSets[i].dstSet = sets[i];
			writeDescSets[i].pImageInfo = &infos[i];
		}
		vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
	}
	
	VkDescriptorSetLayout Swapchain::GetSamplerLayout() const {
		return m_SampleDescriptorLayout;
	}

	VkDescriptorSet Swapchain::GetDepthSampler() const {
		return m_DepthSampleDescriptor;
	}

	VkDescriptorSet Swapchain::GetColorSampler(size_t indx) const {
		return m_ColorImageSampleDescriptors[indx];
	}
}
