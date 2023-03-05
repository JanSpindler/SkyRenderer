#include <cassert>
#include <set>
#include <vulkan/vulkan_core.h>
#include "engine/graphics/AerialPerspective.hpp"
#include "engine/graphics/VulkanAPI.hpp"

#include "aerial_perspective.h"

// one sampler+image store descriptor per image layer.
#define SAMPLE_DESC_COUNT IMAGE_COUNT
#define IMAGE_DESC_COUNT IMAGE_COUNT
#define DESC_COUNT (SAMPLE_DESC_COUNT + IMAGE_DESC_COUNT)

namespace en {

AerialPerspective::AerialPerspective(Camera &cam, Precomputer &precomp, Atmosphere &atm, Sun &sun) :
	m_CommandPool(0, VulkanAPI::GetComputeQFI()),
	m_Shader("sky/aerial_perspective.comp", false),
	m_Cam{cam},
	m_Precomp{precomp},
	m_Atmosphere{atm},
	m_Sun{sun} {
	
	VkDevice device = VulkanAPI::GetDevice();
	// Need command buffer for image transition in CreateComputeImages.
	CreateCommandBuffers();
	CreateComputeImages(device);
	CreateDescriptors(device);
	CreateComputePipeline(device);
	RecordCommandBuffers();
}

AerialPerspective::~AerialPerspective() {
	VkDevice device = VulkanAPI::GetDevice();

	m_CommandPool.Destroy();

	for (int i = 0; i != IMAGE_COUNT; ++i) {
		vkFreeMemory(device, m_APImagesMemory[i], nullptr);
		vkDestroyImage(device, m_APImages[i], nullptr);
		vkDestroyImageView(device, m_APImageViews[i], nullptr);
	}

	vkDestroySampler(device, m_LinearSampler, nullptr);
	vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_ImageDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_SampleDescriptorLayout, nullptr);

	vkDestroyPipelineLayout(device, m_APPipelineLayout, nullptr);
	vkDestroyPipeline(device, m_APPipeline, nullptr);

	m_Shader.Destroy();
}

void AerialPerspective::CreateComputeImages(VkDevice device) {
	std::set<uint32_t> queues = {VulkanAPI::GetComputeQFI(), VulkanAPI::GetGraphicsQFI()}; 
	// vector for contiguous memory.
	std::vector<uint32_t> qvec{queues.begin(), queues.end()};

	m_ComputeImageFormat = VK_FORMAT_R16G16B16A16_SNORM;
	// TODO: do better testing.
	assert(VulkanAPI::IsFormatSupported(
		m_ComputeImageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		0));
	
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D;
	imageInfo.extent.width = AP_X;
	imageInfo.extent.height = AP_Y;
	imageInfo.extent.depth = AP_Z;
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
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

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
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;

	VkCommandBufferBeginInfo beginInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	for (int i = 0; i != IMAGE_COUNT; ++i) {
		ASSERT_VULKAN(vkCreateImage(device, &imageInfo, nullptr, &m_APImages[i]));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, m_APImages[i], &memReqs);

		VkMemoryAllocateInfo memAllocInfo;
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.pNext = nullptr;
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
									   memReqs.memoryTypeBits,
									   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &m_APImagesMemory[i]));
		ASSERT_VULKAN(vkBindImageMemory(device, m_APImages[i], m_APImagesMemory[i], 0));

		imageViewCreateInfo.image = m_APImages[i];
		ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_APImageViews[i]));

		imageMemoryBarrier.image = m_APImages[i];
		ASSERT_VULKAN(vkBeginCommandBuffer(m_LayoutCommandBuffer[i], &beginInfo));

		vkCmdPipelineBarrier(
			m_LayoutCommandBuffer[i],
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);

		ASSERT_VULKAN(vkEndCommandBuffer(m_LayoutCommandBuffer[i]));
	}

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = m_LayoutCommandBuffer.size();
	submitInfo.pCommandBuffers = m_LayoutCommandBuffer.data();
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
	// wait for completion.
	ASSERT_VULKAN(vkQueueWaitIdle(VulkanAPI::GetComputeQueue()));
}

void AerialPerspective::CreateDescriptors(VkDevice device) {
	std::vector<VkDescriptorPoolSize> poolSizes {{
			// read from texture in fragment.
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = SAMPLE_DESC_COUNT
		}, {
			// write to texture in compute.
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = IMAGE_DESC_COUNT
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

	VkDescriptorSetLayoutBinding sampleBindings[SAMPLE_DESC_COUNT];

	std::fill_n(sampleBindings, SAMPLE_DESC_COUNT, VkDescriptorSetLayoutBinding{
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		// sample in fragment.
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	});

	VkDescriptorSetLayoutBinding imageBindings[IMAGE_DESC_COUNT];

	std::fill_n(imageBindings, IMAGE_DESC_COUNT, VkDescriptorSetLayoutBinding{
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		// write in compute.
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	});

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pNext = nullptr;
	layoutCreateInfo.flags = 0;

	layoutCreateInfo.bindingCount = 2;
	layoutCreateInfo.pBindings = sampleBindings;
	sampleBindings[0].binding = 0;
	sampleBindings[1].binding = 1;
	ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_SampleDescriptorLayout));

	layoutCreateInfo.bindingCount = 2;
	layoutCreateInfo.pBindings = imageBindings;
	imageBindings[0].binding = 0;
	imageBindings[1].binding = 1;
	ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_ImageDescriptorLayout));

	VkSamplerCreateInfo sampler;
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.pNext = nullptr;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	// shouldn't address outside of edge, but to be safe.
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

	// each image has one sampler and one imageStoreDescriptor.
	std::vector<VkDescriptorSetLayout> allocLayouts(2);
	std::fill_n(allocLayouts.begin(), 1, m_SampleDescriptorLayout);
	std::fill_n(allocLayouts.begin()+1, 1, m_ImageDescriptorLayout);

	VkDescriptorSetAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = m_DescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
		.pSetLayouts = allocLayouts.data()
	};

	VkDescriptorSet allocTarget[2];
	ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocInfo, allocTarget));

	m_APImageSampleDescriptor = allocTarget[0];
	m_APImageDescriptor = allocTarget[1];

	std::vector<VkWriteDescriptorSet> writeDescSets(IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT);
	std::fill_n(writeDescSets.begin(), IMAGE_DESC_COUNT + SAMPLE_DESC_COUNT, VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr
	});

	std::vector<VkDescriptorImageInfo> texInfo(IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT);
	std::fill_n(texInfo.begin(), IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT, VkDescriptorImageInfo{
		.sampler = m_LinearSampler,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	});

	// TODO: do using loop.
	texInfo[0].imageView = m_APImageViews[AP_TRANSMITTANCE_BINDING];
	writeDescSets[0].pImageInfo = &texInfo[0];
	writeDescSets[0].dstSet = m_APImageDescriptor;
	writeDescSets[0].dstBinding = AP_TRANSMITTANCE_BINDING;
	writeDescSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	texInfo[1].imageView = m_APImageViews[AP_SCATTERING_BINDING];
	writeDescSets[1].pImageInfo = &texInfo[1];
	writeDescSets[1].dstSet = m_APImageDescriptor;
	writeDescSets[1].dstBinding = AP_SCATTERING_BINDING;
	writeDescSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	texInfo[2].imageView = m_APImageViews[AP_TRANSMITTANCE_BINDING];
	writeDescSets[2].pImageInfo = &texInfo[2];
	writeDescSets[2].dstSet = m_APImageSampleDescriptor;
	writeDescSets[2].dstBinding = AP_TRANSMITTANCE_BINDING;
	writeDescSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	texInfo[3].imageView = m_APImageViews[AP_SCATTERING_BINDING];
	writeDescSets[3].pImageInfo = &texInfo[3];
	writeDescSets[3].dstSet = m_APImageSampleDescriptor;
	writeDescSets[3].dstBinding = AP_SCATTERING_BINDING;
	writeDescSets[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
}

void AerialPerspective::CreateCommandBuffers() {
	// one buffer for compute, one for transitioning each image.
	m_CommandPool.AllocateBuffers(1+IMAGE_COUNT, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	std::vector tmp(m_CommandPool.GetBuffers());
	m_ComputeCommandBuffer = tmp[0];
	m_LayoutCommandBuffer[0] = tmp[1];
	m_LayoutCommandBuffer[1] = tmp[2];
}

void AerialPerspective::CreateComputePipeline(VkDevice device) {
	std::vector<VkDescriptorSetLayout> apLayouts(10);
	apLayouts[AP_SETS_IMAGES] = m_ImageDescriptorLayout;
	apLayouts[AP_SETS_CAM] = m_Cam.GetDescriptorSetLayout();
	apLayouts[AP_SETS_SUN] = m_Sun.GetDescriptorSetLayout();
	apLayouts[AP_SETS_RATIO] = m_Precomp.GetRatioDescriptorSetLayout();
	apLayouts[AP_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSetLayout();
	apLayouts[AP_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSetLayout();
	apLayouts[AP_SETS_TRANSMITTANCE_SAMPLER0] = m_Atmosphere.GetTransmittanceSampleDescriptorLayout();
	apLayouts[AP_SETS_TRANSMITTANCE_SAMPLER1] = m_Atmosphere.GetTransmittanceSampleDescriptorLayout();
	apLayouts[AP_SETS_GATHERING_SAMPLER0] = m_Atmosphere.GetGatheringSampleDescriptorLayout();
	apLayouts[AP_SETS_GATHERING_SAMPLER1] = m_Atmosphere.GetGatheringSampleDescriptorLayout();

	VkPipelineLayoutCreateInfo apLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.setLayoutCount = static_cast<uint32_t>(apLayouts.size()),
		.pSetLayouts = apLayouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};
	ASSERT_VULKAN(vkCreatePipelineLayout(device, &apLayoutCreateInfo, nullptr, &m_APPipelineLayout));

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
	pipeline.stage.module = m_Shader.GetVulkanModule();
	pipeline.layout = m_APPipelineLayout;

	ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_APPipeline));
}

void AerialPerspective::RecordCommandBuffers() {
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	ASSERT_VULKAN(vkBeginCommandBuffer(m_ComputeCommandBuffer, &beginInfo));

	vkCmdBindPipeline(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_APPipeline);

	std::vector<VkDescriptorSet> sets(10);
	sets[AP_SETS_IMAGES] = m_APImageDescriptor;
	sets[AP_SETS_CAM] = m_Cam.GetDescriptorSet();
	sets[AP_SETS_SUN] = m_Sun.GetDescriptorSet();
	sets[AP_SETS_RATIO] = m_Precomp.GetRatioDescriptorSet();
	sets[AP_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSet();
	sets[AP_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSet();
	sets[AP_SETS_TRANSMITTANCE_SAMPLER0] = m_Atmosphere.GetTransmittanceSampleDescriptorSet(0);
	sets[AP_SETS_TRANSMITTANCE_SAMPLER1] = m_Atmosphere.GetTransmittanceSampleDescriptorSet(1);
	sets[AP_SETS_GATHERING_SAMPLER0] = m_Atmosphere.GetGatheringSampleDescriptorSet(0);
	sets[AP_SETS_GATHERING_SAMPLER1] = m_Atmosphere.GetGatheringSampleDescriptorSet(1);

	vkCmdBindDescriptorSets(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_APPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

	// each shader walks along Z, so only 1 instance per (x,y) is needed.
	vkCmdDispatch(m_ComputeCommandBuffer, AP_X, AP_Y, 1);

	vkEndCommandBuffer(m_ComputeCommandBuffer);
}

void AerialPerspective::Compute(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = waitSemaphore != nullptr ? 1 : 0;
	submitInfo.pWaitSemaphores = waitSemaphore;
	submitInfo.pWaitDstStageMask = &waitFlags;
	submitInfo.signalSemaphoreCount = signalSemaphore != nullptr ? 1 : 0;
	submitInfo.pSignalSemaphores = signalSemaphore;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_ComputeCommandBuffer;

	ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
}

VkDescriptorSet AerialPerspective::GetSampleDescriptor() const {
	return m_APImageSampleDescriptor;
}

VkDescriptorSetLayout AerialPerspective::GetSampleDescriptorLayout() const {
	return m_SampleDescriptorLayout;
}

}
