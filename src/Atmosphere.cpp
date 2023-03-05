#include <algorithm>
#include <cassert>
#include <engine/graphics/Common.hpp>
#include "engine/graphics/VulkanAPI.hpp"
#include <engine/graphics/Atmosphere.hpp>
#include <imgui.h>
#include <set>
#include <vulkan/vulkan_core.h>

#include <scattering.h>
#include <multi_scattering.h>
#include <sum.h>
#include <gathering.h>
#include <transmittance.h>

// One for a scattering/gathering orders, two for sum (switching!)
#define SCATTERING_IMAGE_COUNT (1+2)
#define GATHERING_IMAGE_COUNT (1+2)
#define TRANSMITTANCE_IMAGE_COUNT 2

#define IMAGE_COUNT (GATHERING_IMAGE_COUNT+SCATTERING_IMAGE_COUNT+TRANSMITTANCE_IMAGE_COUNT)

namespace en {
	Atmosphere::Atmosphere(VkDescriptorSetLayout env) :
		m_ComputeCommandPool(0, VulkanAPI::GetComputeQFI()),
		m_LayoutCommandPool(0, VulkanAPI::GetComputeQFI()),
		m_SingleShader("sky/single_scattering.comp", false),
		m_MultiShader("sky/multi_scattering.comp", false),
		m_GatheringShader("sky/gathering.comp", false),
		m_TransmittanceShader("sky/transmittance.comp", false),
		m_EnvLayout{env}
	{
		VkDevice device = VulkanAPI::GetDevice();

		CreateCommandBuffers();
		CreateComputeImages(device);
		CreateDescriptors(device);

		CreateComputePipeline(device);
		// RecordGatheringCommandBuffer(m_GatheringBuffer, 0, GATHERING_RESOLUTION_HEIGHT);
		// RecordSingleScatteringCommandBuffer(m_SingleScatteringBuffer, 0, SCATTERING_RESOLUTION_HEIGHT);
		// RecordSingleScatteringCommandBuffer(m_MultiScatteringBuffer, 0, SCATTERING_RESOLUTION_HEIGHT);

		// Precompute();
	}

	Atmosphere::~Atmosphere() {
		VkDevice device = VulkanAPI::GetDevice();

		m_ComputeCommandPool.Destroy();
		m_LayoutCommandPool.Destroy();

		vkFreeMemory(device, m_ScatteringImageMemory, nullptr);
		vkDestroyImage(device, m_ScatteringImage, nullptr);
		vkDestroyImageView(device, m_ScatteringImageView, nullptr);

		vkFreeMemory(device, m_ScatteringSumImageMemory[0], nullptr);
		vkDestroyImage(device, m_ScatteringSumImage[0], nullptr);
		vkDestroyImageView(device, m_ScatteringSumImageView[0], nullptr);
		vkFreeMemory(device, m_ScatteringSumImageMemory[1], nullptr);
		vkDestroyImage(device, m_ScatteringSumImage[1], nullptr);
		vkDestroyImageView(device, m_ScatteringSumImageView[1], nullptr);


		vkFreeMemory(device, m_GatheringImageMemory, nullptr);
		vkDestroyImage(device, m_GatheringImage, nullptr);
		vkDestroyImageView(device, m_GatheringImageView, nullptr);

		vkFreeMemory(device, m_GatheringSumImageMemory[0], nullptr);
		vkDestroyImage(device, m_GatheringSumImage[0], nullptr);
		vkDestroyImageView(device, m_GatheringSumImageView[0], nullptr);
		vkFreeMemory(device, m_GatheringSumImageMemory[1], nullptr);
		vkDestroyImage(device, m_GatheringSumImage[1], nullptr);
		vkDestroyImageView(device, m_GatheringSumImageView[1], nullptr);

		vkFreeMemory(device, m_TransmittanceImageMemory[0], nullptr);
		vkDestroyImage(device, m_TransmittanceImage[0], nullptr);
		vkDestroyImageView(device, m_TransmittanceImageView[0], nullptr);
		vkFreeMemory(device, m_TransmittanceImageMemory[1], nullptr);
		vkDestroyImage(device, m_TransmittanceImage[1], nullptr);
		vkDestroyImageView(device, m_TransmittanceImageView[1], nullptr);

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_ImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, m_SampleDescriptorLayout, nullptr);

		vkDestroySampler(device, m_LinearSampler, nullptr);

		vkDestroyPipelineLayout(device, m_SSPipelineLayout, nullptr);
		vkDestroyPipeline(device, m_SSPipeline, nullptr);

		vkDestroyPipelineLayout(device, m_MSPipelineLayout, nullptr);
		vkDestroyPipeline(device, m_MSPipeline, nullptr);

		vkDestroyPipelineLayout(device, m_GPipelineLayout, nullptr);
		vkDestroyPipeline(device, m_GPipeline, nullptr);

		vkDestroyPipelineLayout(device, m_TPipelineLayout, nullptr);
		vkDestroyPipeline(device, m_TPipeline, nullptr);

		m_SingleShader.Destroy();
		m_MultiShader.Destroy();
		m_GatheringShader.Destroy();
		m_TransmittanceShader.Destroy();
	}

	void Atmosphere::Precompute() const
	{
		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_SingleScatteringBuffer;

		ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
		vkQueueWaitIdle(VulkanAPI::GetComputeQueue());

		submitInfo.pCommandBuffers = &m_GatheringBuffer;
		ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
		vkQueueWaitIdle(VulkanAPI::GetComputeQueue());

		for (int i = 1; i != SCATTERING_ORDERS; ++i) {
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_MultiScatteringBuffer;

			ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
			vkQueueWaitIdle(VulkanAPI::GetComputeQueue());

			submitInfo.pCommandBuffers = &m_GatheringBuffer;
			ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
			vkQueueWaitIdle(VulkanAPI::GetComputeQueue());
		}
	}

	void Atmosphere::CreateDescriptors(VkDevice device)
	{
		std::vector<VkDescriptorPoolSize> poolSizes {{
				// read from texture in compute/fragment.
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				// Need to sample from all images.
				.descriptorCount = IMAGE_COUNT
			}, {
				// write to texture in compute.
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				// one descriptor per image.
				.descriptorCount = IMAGE_COUNT
			}
		};

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = poolSizes[0].descriptorCount + poolSizes[1].descriptorCount;
		descPoolCreateInfo.poolSizeCount = poolSizes.size();
		descPoolCreateInfo.pPoolSizes = poolSizes.data();

		ASSERT_VULKAN(vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool));

		VkDescriptorSetLayoutBinding sampleBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			// Gathering and transmittance have to be sampled in compute, scattering in comp+frag.
			// Could be split up, but prob not worth the effort.
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutBinding imageBinding {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			// access image for scattering.
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.flags = 0;

		layoutCreateInfo.pBindings = &sampleBinding;
		ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_SampleDescriptorLayout));

		layoutCreateInfo.pBindings = &imageBinding;
		ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_ImageDescriptorLayout));

		std::vector<VkDescriptorSetLayout> allocLayouts(2*IMAGE_COUNT);
		// create IMAGE_COUNT store_images and samplers.
		std::fill_n(allocLayouts.begin(), IMAGE_COUNT, m_SampleDescriptorLayout);
		std::fill_n(allocLayouts.begin()+IMAGE_COUNT, IMAGE_COUNT, m_ImageDescriptorLayout);

		VkDescriptorSetAllocateInfo allocInfo {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
			.pSetLayouts = allocLayouts.data()
		};

		// make space for both descriptor sets.
		std::vector<VkDescriptorSet> allocTarget(2*IMAGE_COUNT);
		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, allocTarget.data()));
		// extract descriptors corresponding to contents of allocLayouts.
		m_ScatteringSampleDescriptor = allocTarget[0];
		m_ScatteringSumSampleDescriptor[0] = allocTarget[1];
		m_ScatteringSumSampleDescriptor[1] = allocTarget[2];
		m_GatheringSampleDescriptor = allocTarget[3];
		m_GatheringSumSampleDescriptor[0] = allocTarget[4];
		m_GatheringSumSampleDescriptor[1] = allocTarget[5];
		m_TransmittanceSampleDescriptor[0] = allocTarget[6];
		m_TransmittanceSampleDescriptor[1] = allocTarget[7];

		m_ScatteringImageDescriptor = allocTarget[IMAGE_COUNT+0];
		m_ScatteringSumImageDescriptor[0] = allocTarget[IMAGE_COUNT+1];
		m_ScatteringSumImageDescriptor[1] = allocTarget[IMAGE_COUNT+2];
		m_GatheringImageDescriptor = allocTarget[IMAGE_COUNT+3];
		m_GatheringSumImageDescriptor[0] = allocTarget[IMAGE_COUNT+4];
		m_GatheringSumImageDescriptor[1] = allocTarget[IMAGE_COUNT+5];
		m_TransmittanceImageDescriptor[0] = allocTarget[IMAGE_COUNT+6];
		m_TransmittanceImageDescriptor[1] = allocTarget[IMAGE_COUNT+7];

		std::vector<VkDescriptorImageInfo> texInfo(2*IMAGE_COUNT);
		std::fill_n(texInfo.begin(), 2*IMAGE_COUNT, VkDescriptorImageInfo{
			.sampler = m_LinearSampler,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		});

		// prepare common members.
		std::vector<VkWriteDescriptorSet> writeDescSets(2*IMAGE_COUNT);
		std::fill_n(writeDescSets.begin(), 2*IMAGE_COUNT, VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = 0,
			.dstArrayElement = 0,
			// cannot update multiple with the same ImageInfo, different dstSets.
			.descriptorCount = 1,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr
		});

		std::vector<VkImageView> imageViews = {
			m_ScatteringImageView,
			m_ScatteringSumImageView[0],
			m_ScatteringSumImageView[1],
			m_GatheringImageView,
			m_GatheringSumImageView[0],
			m_GatheringSumImageView[1],
			m_TransmittanceImageView[0],
			m_TransmittanceImageView[1]
		};

		std::vector<VkDescriptorSet> imageDests = {
			m_ScatteringImageDescriptor,
			m_ScatteringSumImageDescriptor[0],
			m_ScatteringSumImageDescriptor[1],
			m_GatheringImageDescriptor,
			m_GatheringSumImageDescriptor[0],
			m_GatheringSumImageDescriptor[1],
			m_TransmittanceImageDescriptor[0],
			m_TransmittanceImageDescriptor[1]
		};

		std::vector<VkDescriptorSet> sampleDests = {
			m_ScatteringSampleDescriptor,
			m_ScatteringSumSampleDescriptor[0],
			m_ScatteringSumSampleDescriptor[1],
			m_GatheringSampleDescriptor,
			m_GatheringSumSampleDescriptor[0],
			m_GatheringSumSampleDescriptor[1],
			m_TransmittanceSampleDescriptor[0],
			m_TransmittanceSampleDescriptor[1]
		};

		for (int i = 0; i != IMAGE_COUNT; ++i) {
			texInfo[i].imageView = imageViews[i];

			writeDescSets[i].dstSet = imageDests[i];
			writeDescSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescSets[i].pImageInfo = &texInfo[i];

			writeDescSets[IMAGE_COUNT + i].dstSet = sampleDests[i];
			writeDescSets[IMAGE_COUNT + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescSets[IMAGE_COUNT + i].pImageInfo = &texInfo[i];
		}
		vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
	}

	void Atmosphere::CreateComputeImages(VkDevice device)
	{
		std::set<uint32_t> queues = {VulkanAPI::GetComputeQFI(), VulkanAPI::GetGraphicsQFI()}; 
		// vector for contiguous memory.
		std::vector<uint32_t> qvec{queues.begin(), queues.end()};

		m_ComputeImageFormat = VK_FORMAT_R16G16B16A16_SNORM;
		// do better testing.
		assert(VulkanAPI::IsFormatSupported(
			m_ComputeImageFormat,
			VK_IMAGE_TILING_OPTIMAL,
			0));

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_3D;
		imageInfo.extent.width = SCATTERING_RESOLUTION_HEIGHT;
		imageInfo.extent.height = SCATTERING_RESOLUTION_VIEW;
		imageInfo.extent.depth = SCATTERING_RESOLUTION_SUN;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_ComputeImageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		// will only be accessed by one queue at a time (for now).
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.queueFamilyIndexCount = qvec.size();
		imageInfo.pQueueFamilyIndices = qvec.data();

		VkMemoryAllocateInfo memAllocInfo;
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.pNext = nullptr;

		std::vector<VkImage> scatteringImages(SCATTERING_IMAGE_COUNT);
		std::vector<VkDeviceMemory> scatteringImageMemory(SCATTERING_IMAGE_COUNT);

		// TODO: allocate into one block, pass offsets to vkBindImageMemory.
		for (int i = 0; i != SCATTERING_IMAGE_COUNT; ++i) {
			ASSERT_VULKAN(vkCreateImage(device, &imageInfo, nullptr, &scatteringImages[i]));

			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, scatteringImages[i], &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
				memReqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &scatteringImageMemory[i]));
			ASSERT_VULKAN(vkBindImageMemory(device, scatteringImages[i], scatteringImageMemory[i], 0));
		}
		m_ScatteringImage = scatteringImages[0];
		m_ScatteringSumImage[0] = scatteringImages[1];
		m_ScatteringSumImage[1] = scatteringImages[2];

		m_ScatteringImageMemory = scatteringImageMemory[0];
		m_ScatteringSumImageMemory[0] = scatteringImageMemory[1];
		m_ScatteringSumImageMemory[1] = scatteringImageMemory[2];


		std::vector<VkImage> gatheringImages(GATHERING_IMAGE_COUNT);
		std::vector<VkDeviceMemory> gatheringImageMemory(GATHERING_IMAGE_COUNT);

		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = GATHERING_RESOLUTION_HEIGHT;
		imageInfo.extent.height = GATHERING_RESOLUTION_SUN;
		imageInfo.extent.depth = 1;
		for (int i = 0; i != GATHERING_IMAGE_COUNT; ++i) {
			ASSERT_VULKAN(vkCreateImage(device, &imageInfo, nullptr, &gatheringImages[i]));

			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, gatheringImages[i], &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
				memReqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &gatheringImageMemory[i]));
			ASSERT_VULKAN(vkBindImageMemory(device, gatheringImages[i], gatheringImageMemory[i], 0));
		}
		m_GatheringImage = gatheringImages[0];
		m_GatheringSumImage[0] = gatheringImages[1];
		m_GatheringSumImage[1] = gatheringImages[2];

		m_GatheringImageMemory = gatheringImageMemory[0];
		m_GatheringSumImageMemory[0] = gatheringImageMemory[1];
		m_GatheringSumImageMemory[1] = gatheringImageMemory[2];


		std::vector<VkImage> transmittanceImages(TRANSMITTANCE_IMAGE_COUNT);
		std::vector<VkDeviceMemory> transmittanceImageMemory(TRANSMITTANCE_IMAGE_COUNT);

		imageInfo.extent.width = TRANSMITTANCE_RESOLUTION_HEIGHT;
		imageInfo.extent.height = TRANSMITTANCE_RESOLUTION_VIEW;
		imageInfo.extent.depth = 1;
		for (int i = 0; i != TRANSMITTANCE_IMAGE_COUNT; ++i) {
			ASSERT_VULKAN(vkCreateImage(device, &imageInfo, nullptr, &transmittanceImages[i]));

			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, transmittanceImages[i], &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
				memReqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &transmittanceImageMemory[i]));
			ASSERT_VULKAN(vkBindImageMemory(device, transmittanceImages[i], transmittanceImageMemory[i], 0));
		}
		m_TransmittanceImage[0] = transmittanceImages[0];
		m_TransmittanceImage[1] = transmittanceImages[1];

		m_TransmittanceImageMemory[0] = transmittanceImageMemory[0];
		m_TransmittanceImageMemory[1] = transmittanceImageMemory[1];

		// Create sampler
		VkSamplerCreateInfo sampler;
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.pNext = nullptr;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.mipLodBias = 0.0f;
		sampler.anisotropyEnable = VK_FALSE;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareEnable = VK_FALSE;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		sampler.unnormalizedCoordinates = VK_FALSE;
		sampler.flags = 0;
		ASSERT_VULKAN(vkCreateSampler(device, &sampler, nullptr, &m_LinearSampler));

		// Create Image View
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.format = m_ComputeImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		std::vector<VkImageView> scatteringImageViews(SCATTERING_IMAGE_COUNT);
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		for (int i = 0; i != SCATTERING_IMAGE_COUNT; ++i) {
			imageViewCreateInfo.image = scatteringImages[i];

			ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &scatteringImageViews[i]));
		}
		m_ScatteringImageView = scatteringImageViews[0];
		m_ScatteringSumImageView[0] = scatteringImageViews[1];
		m_ScatteringSumImageView[1] = scatteringImageViews[2];

		std::vector<VkImageView> gatheringImageViews(SCATTERING_IMAGE_COUNT);
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		for (int i = 0; i != GATHERING_IMAGE_COUNT; ++i) {
			imageViewCreateInfo.image = gatheringImages[i];

			ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &gatheringImageViews[i]));
		}
		m_GatheringImageView = gatheringImageViews[0];
		m_GatheringSumImageView[0] = gatheringImageViews[1];
		m_GatheringSumImageView[1] = gatheringImageViews[2];

		std::vector<VkImageView> transmittanceImageViews(TRANSMITTANCE_IMAGE_COUNT);
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		for (int i = 0; i != TRANSMITTANCE_IMAGE_COUNT; ++i) {
			imageViewCreateInfo.image = transmittanceImages[i];

			ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &transmittanceImageViews[i]));
		}
		m_TransmittanceImageView[0] = transmittanceImageViews[0];
		m_TransmittanceImageView[1] = transmittanceImageViews[1];

		// use compute-command buffer once here.
		VkCommandBufferBeginInfo beginInfo {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pInheritanceInfo = nullptr
		};

		VkImageMemoryBarrier imageMemoryBarrier;
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext = nullptr;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE_KHR;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // TODO: manage for later usage
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = 1;

		// append gathering and transmittance images.
		scatteringImages.insert(scatteringImages.end(), gatheringImages.begin(), gatheringImages.end());
		scatteringImages.insert(scatteringImages.end(), transmittanceImages.begin(), transmittanceImages.end());
		for (int i = 0; i != IMAGE_COUNT; ++i) {
			imageMemoryBarrier.image = scatteringImages[i];
			ASSERT_VULKAN(vkBeginCommandBuffer(m_LayoutCommandBuffers[i], &beginInfo));

			vkCmdPipelineBarrier(
				m_LayoutCommandBuffers[i],
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);

			ASSERT_VULKAN(vkEndCommandBuffer(m_LayoutCommandBuffers[i]));
		}

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		// submit all buffers at once.
		submitInfo.commandBufferCount = m_LayoutCommandBuffers.size();
		submitInfo.pCommandBuffers = m_LayoutCommandBuffers.data();
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));

		ASSERT_VULKAN(vkQueueWaitIdle(VulkanAPI::GetComputeQueue()));
	}

	void Atmosphere::CreateComputePipeline(VkDevice device)
	{
		VkPushConstantRange pcRange = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof(uint32_t),
		};

		std::vector<VkDescriptorSetLayout> ssLayouts(4);
		ssLayouts[SS_SETS_TRANSMITTANCE_SAMPLER] = m_SampleDescriptorLayout;
		ssLayouts[SS_SETS_SCATTERING_IMAGE] = m_ImageDescriptorLayout;
		ssLayouts[SS_SETS_SUM_IMAGE] = m_ImageDescriptorLayout;
		ssLayouts[SS_SETS_ENV] = m_EnvLayout;

		VkPipelineLayoutCreateInfo ssLayoutCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.setLayoutCount = static_cast<uint32_t>(ssLayouts.size()),
			.pSetLayouts = ssLayouts.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pcRange
		};
		ASSERT_VULKAN(vkCreatePipelineLayout(device, &ssLayoutCreateInfo, nullptr, &m_SSPipelineLayout));

		std::vector<VkDescriptorSetLayout> msLayouts(5);
		msLayouts[MS_SETS_GATHERING_SAMPLER] = m_SampleDescriptorLayout;
		msLayouts[MS_SETS_TRANSMITTANCE_SAMPLER] = m_SampleDescriptorLayout;
		msLayouts[MS_SETS_SUM_IMAGE] = m_ImageDescriptorLayout;
		msLayouts[MS_SETS_SCATTERING_IMAGE] = m_ImageDescriptorLayout;
		msLayouts[MS_SETS_ENV] = m_EnvLayout;

		VkPipelineLayoutCreateInfo msLayoutCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.setLayoutCount = static_cast<uint32_t>(msLayouts.size()),
			.pSetLayouts = msLayouts.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pcRange
		};
		ASSERT_VULKAN(vkCreatePipelineLayout(device, &msLayoutCreateInfo, nullptr, &m_MSPipelineLayout));

		std::vector<VkDescriptorSetLayout> gLayouts(5);
		gLayouts[G_SETS_SCATTERING_SAMPLER] = m_SampleDescriptorLayout;
		gLayouts[G_SETS_TRANSMITTANCE_SAMPLER] = m_SampleDescriptorLayout;
		gLayouts[G_SETS_SUM_IMAGE] = m_ImageDescriptorLayout;
		gLayouts[G_SETS_GATHERING_IMAGE] = m_ImageDescriptorLayout;
		gLayouts[G_SETS_ENV] = m_EnvLayout;

		VkPipelineLayoutCreateInfo gLayoutCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.setLayoutCount = static_cast<uint32_t>(gLayouts.size()),
			.pSetLayouts = gLayouts.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pcRange
		};
		ASSERT_VULKAN(vkCreatePipelineLayout(device, &gLayoutCreateInfo, nullptr, &m_GPipelineLayout));

		std::vector<VkDescriptorSetLayout> tLayouts(2);
		tLayouts[T_SETS_IMAGE] = m_ImageDescriptorLayout;
		tLayouts[T_SETS_ENV] = m_EnvLayout;

		VkPipelineLayoutCreateInfo tLayoutCreateInfo {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.setLayoutCount = static_cast<uint32_t>(tLayouts.size()),
			.pSetLayouts = tLayouts.data(),
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr
		};
		ASSERT_VULKAN(vkCreatePipelineLayout(device, &tLayoutCreateInfo, nullptr, &m_TPipelineLayout));

		VkPipelineShaderStageCreateInfo compStageCreateInfo;
		compStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compStageCreateInfo.pNext = nullptr;
		compStageCreateInfo.flags = 0;
		compStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compStageCreateInfo.pName = "main";
		compStageCreateInfo.pSpecializationInfo = nullptr;

		VkComputePipelineCreateInfo pipeline;
		pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline.pNext = nullptr;
		pipeline.flags = 0;
		pipeline.basePipelineHandle = VK_NULL_HANDLE;
		pipeline.stage = compStageCreateInfo;

		pipeline.stage.module = m_SingleShader.GetVulkanModule();
		pipeline.layout = m_SSPipelineLayout;
		ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_SSPipeline));

		pipeline.stage.module = m_MultiShader.GetVulkanModule();
		pipeline.layout = m_MSPipelineLayout;
		ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_MSPipeline));

		pipeline.stage.module = m_GatheringShader.GetVulkanModule();
		pipeline.layout = m_GPipelineLayout;
		ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_GPipeline));

		pipeline.stage.module = m_TransmittanceShader.GetVulkanModule();
		pipeline.layout = m_TPipelineLayout;
		ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_TPipeline));
	}

	void Atmosphere::CreateCommandBuffers()
	{
		// one buffer for multi/single scattering, one for gathering.
		m_ComputeCommandPool.AllocateBuffers(1+1+1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		std::vector tmp(m_ComputeCommandPool.GetBuffers());
		// All but last one.
		m_SingleScatteringBuffer = tmp[0];
		m_MultiScatteringBuffer = tmp[1];
		m_GatheringBuffer = tmp[2];

		// Buffers for transitioning layout, one per image (+1 for sum-image).
		m_LayoutCommandPool.AllocateBuffers(IMAGE_COUNT, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		m_LayoutCommandBuffers = m_LayoutCommandPool.GetBuffers();
	}

	void Atmosphere::RecordTransmittanceCommandBuffer(VkCommandBuffer buf, size_t sum_target, VkDescriptorSet env)
	{
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		ASSERT_VULKAN(vkBeginCommandBuffer(buf, &beginInfo));

		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_TPipeline);

		std::vector<VkDescriptorSet> sets(2);
		sets[T_SETS_IMAGE] = m_TransmittanceImageDescriptor[sum_target];
		sets[T_SETS_ENV] = env;
		vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_TPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

		vkCmdDispatch(buf, TRANSMITTANCE_RESOLUTION_HEIGHT, TRANSMITTANCE_RESOLUTION_VIEW, 1);

		vkEndCommandBuffer(buf);
	}

	void Atmosphere::RecordSingleScatteringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env)
	{
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		// Record all into separate buffers to execute sequentially for now.

		// single scattering
		ASSERT_VULKAN(vkBeginCommandBuffer(buf, &beginInfo));

		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_SSPipeline);

		std::vector<VkDescriptorSet> sets(4);
		sets[SS_SETS_TRANSMITTANCE_SAMPLER] = m_TransmittanceSampleDescriptor[sum_target];
		sets[SS_SETS_SCATTERING_IMAGE] = m_ScatteringImageDescriptor;
		sets[SS_SETS_SUM_IMAGE] = m_ScatteringSumImageDescriptor[sum_target];
		sets[SS_SETS_ENV] = env;
		vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_SSPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);
		vkCmdPushConstants(buf, m_SSPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &offset);

		vkCmdDispatch(buf, count, SCATTERING_RESOLUTION_VIEW, SCATTERING_RESOLUTION_SUN);

		vkEndCommandBuffer(buf);
	}

	void Atmosphere::RecordMultiScatteringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env)
	{
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		ASSERT_VULKAN(vkBeginCommandBuffer(buf, &beginInfo));

		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_MSPipeline);

		std::vector<VkDescriptorSet> sets(5);
		sets[MS_SETS_TRANSMITTANCE_SAMPLER] = m_TransmittanceSampleDescriptor[sum_target];
		sets[MS_SETS_SUM_IMAGE] = m_ScatteringSumImageDescriptor[sum_target];
		sets[MS_SETS_SCATTERING_IMAGE] = m_ScatteringImageDescriptor;
		// sample gathered light of previous order.
		sets[MS_SETS_GATHERING_SAMPLER] = m_GatheringSampleDescriptor;
		sets[MS_SETS_ENV] = env;

		vkCmdBindDescriptorSets(buf,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			m_MSPipelineLayout,
			0,
			sets.size(),
			sets.data(),
			0,
			nullptr);
		vkCmdPushConstants(buf, m_MSPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &offset);
		vkCmdDispatch(buf,
			count,
			SCATTERING_RESOLUTION_VIEW,
			SCATTERING_RESOLUTION_SUN);

		vkEndCommandBuffer(buf);
	}

	void Atmosphere::RecordGatheringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env) {
		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		ASSERT_VULKAN(vkBeginCommandBuffer(buf, &beginInfo));

		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_GPipeline);
		// always 3 descriptorSets from now on.
		std::vector<VkDescriptorSet> sets(5);
		sets[G_SETS_TRANSMITTANCE_SAMPLER] = m_TransmittanceSampleDescriptor[sum_target];
		// sample from previous scattering order.
		sets[G_SETS_SCATTERING_SAMPLER] = m_ScatteringSampleDescriptor;
		sets[G_SETS_SUM_IMAGE] = m_GatheringSumImageDescriptor[sum_target];
		sets[G_SETS_GATHERING_IMAGE] = m_GatheringImageDescriptor;
		sets[G_SETS_ENV] = env;

		vkCmdBindDescriptorSets(buf,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			m_GPipelineLayout,
			0,
			sets.size(),
			sets.data(),
			0,
			nullptr);
		vkCmdPushConstants(buf, m_GPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &offset);
		vkCmdDispatch(buf,
			count,
			GATHERING_RESOLUTION_SUN,
			1);

		vkEndCommandBuffer(buf);
	}

	// all three use the same layout, but that may change, so different functions make sense for now.
	VkDescriptorSetLayout Atmosphere::GetScatteringSampleDescriptorLayout() { return m_SampleDescriptorLayout; }
	VkDescriptorSet Atmosphere::GetScatteringSampleDescriptorSet(size_t sum_target) {  return m_ScatteringSumSampleDescriptor[sum_target]; }

	VkDescriptorSetLayout Atmosphere::GetTransmittanceSampleDescriptorLayout() {  return m_SampleDescriptorLayout; }
	VkDescriptorSet Atmosphere::GetTransmittanceSampleDescriptorSet(size_t sum_target) {  return m_TransmittanceSampleDescriptor[sum_target]; }

	VkDescriptorSetLayout Atmosphere::GetGatheringSampleDescriptorLayout() {  return m_SampleDescriptorLayout; }
	VkDescriptorSet Atmosphere::GetGatheringSampleDescriptorSet(size_t sum_target) {  return m_GatheringSumSampleDescriptor[sum_target]; }
}
