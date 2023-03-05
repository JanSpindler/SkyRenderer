#include <cassert>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <math.h>
#include <set>
#include <vulkan/vulkan_core.h>
#include "engine/graphics/GroundLighting.hpp"
#include "engine/graphics/VulkanAPI.hpp"
#include <iostream>
#include <fstream>
#include <numeric>

#include "ground_lighting.h"

// one sampler+image store descriptor per image layer.
#define SAMPLE_DESC_COUNT 1
#define IMAGE_DESC_COUNT 1
#define DESC_COUNT (SAMPLE_DESC_COUNT + IMAGE_DESC_COUNT)

namespace en {

GroundLighting::GroundLighting(Precomputer &precomp, Atmosphere &atm, Sun &sun) :
	m_CommandPool(0, VulkanAPI::GetComputeQFI()),
	m_Precomp{precomp},
	m_Atmosphere{atm},
	m_Sun{sun} {
	
	GenerateVectors();
	m_Shader = vk::Shader("sky/ground_lighting.comp", false);
	VkDevice device = VulkanAPI::GetDevice();
	// Need command buffer for image transition in CreateComputeImages.
	CreateCommandBuffers();
	CreateComputeImages(device);
	CreateDescriptors(device);
	CreateComputePipeline(device);
	RecordCommandBuffers();
}

GroundLighting::~GroundLighting() {
	VkDevice device = VulkanAPI::GetDevice();

	m_CommandPool.Destroy();

	vkFreeMemory(device, m_GLImagesMemory, nullptr);
	vkDestroyImage(device, m_GLImage, nullptr);
	vkDestroyImageView(device, m_CubeImageView, nullptr);
	vkDestroyImageView(device, m_ImageView, nullptr);

	vkDestroySampler(device, m_LinearSampler, nullptr);
	vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device, m_ImageDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, m_SampleDescriptorLayout, nullptr);

	vkDestroyPipelineLayout(device, m_GLPipelineLayout, nullptr);
	vkDestroyPipeline(device, m_GLPipeline, nullptr);

	m_Shader.Destroy();
}

std::ostream &operator<<(std::ostream &file, const std::vector<glm::vec3> &vecs) {
	file << "{\n";
	file << "\t\t" << glm::to_string(vecs[0]);
	for (int i = 1; i != vecs.size(); ++i)
		file << ",\n\t\t" << glm::to_string(vecs[i]);
	file << "\n\t}";
	return file;
}

// construct copy.
std::ostream &operator<<(std::ostream &file, const std::vector<std::vector<glm::vec3>> &all_vecs) {
	file << "const vec3 hemisphere_vecs[" << all_vecs.size() << "][" << all_vecs[0].size() << "] = {\n\t";
	file << all_vecs[0];
	for (int i = 1; i != all_vecs.size(); ++i)
		file << ", " << all_vecs[i];
	file << "\n};";
	return file;
}

void GroundLighting::GenerateVectors() {
	glm::vec3 vec(0,1,0);

	// most steps are taken at the belly of the sphere, sin(90) = 1.
	int max_hor_steps = 2*M_PI/GRL_VEC_HOR_DIST;

	std::vector<std::vector<glm::vec3>> all_vecs(GRL_ZENITH_ANGLES+2);
	all_vecs[0] = std::vector<glm::vec3>(max_hor_steps);
	all_vecs[0][0] = glm::vec3(0,1,0);

	std::vector<int> hor_counts(GRL_ZENITH_ANGLES+2);
	hor_counts[0] = 1;

	for (int i = 1; i != GRL_ZENITH_ANGLES+1; ++i) {
		// don't sample vec3(0,-1,0) GRL_AZIMUTH_ANGLES-times.
		float zenith_step = float(M_PI_2)/(GRL_ZENITH_ANGLES+1);
		// azimuth has to complete one circle.
		glm::mat3 zenith_rot = glm::mat3(glm::rotate(zenith_step, glm::vec3(1,0,0)));

		vec = zenith_rot * vec;

		// calculate azimuth-angle-step based on circumference of sphere at given zenith angle.
		float zen_circ = sin(zenith_step*i)*2*M_PI;
		int az_steps = int(zen_circ/GRL_VEC_HOR_DIST);
		float az_step = float(2*M_PI)/az_steps;

		hor_counts[i] = az_steps;
		glm::mat3 azimuth_rot = glm::mat3(glm::rotate(az_step, glm::vec3(0,1,0)));

		// default-inserted elements, some overriden subsequently, the rest don't matter.
		std::vector<glm::vec3> vecs(max_hor_steps);
		glm::vec3 az_vec = vec;
		for (int j = 0; j != az_steps; ++j) {
			az_vec = azimuth_rot * az_vec;
			vecs[j] = az_vec;
		}
		all_vecs[i] = vecs;
	}
	all_vecs[GRL_ZENITH_ANGLES+1] = std::vector<glm::vec3>(max_hor_steps);
	all_vecs[GRL_ZENITH_ANGLES+1][0] = glm::vec3(0,-1,0);
	hor_counts[GRL_ZENITH_ANGLES+1] = 1;

	std::ofstream vec_header;
	vec_header.open("data/shader/generated/hemisphere_vecs.glsl");
	vec_header << "const int hemisphere_hor_sizes[GRL_ZENITH_ANGLES+2] = {\n";
	vec_header << "\t" << hor_counts[0];
	for (int i = 1; i != GRL_ZENITH_ANGLES+2; ++i) {
		vec_header << ",\n\t" << hor_counts[i];
	}
	vec_header << "\n};\n";
	vec_header << "const int hemisphere_vec_count = " << std::accumulate(hor_counts.begin(), hor_counts.end(), 0) << ";\n";
	vec_header << all_vecs;
	vec_header.close();
}

void GroundLighting::CreateComputeImages(VkDevice device) {
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
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageInfo.extent.width = GRL_X;
	imageInfo.extent.height = GRL_Y;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = CUBE_FACES;
	imageInfo.format = m_ComputeImageFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	// will only be accessed by one queue at a time (for now).
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = qvec.size();
	imageInfo.pQueueFamilyIndices = qvec.data();

	ASSERT_VULKAN(vkCreateImage(device, &imageInfo, nullptr, &m_GLImage));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, m_GLImage, &memReqs);

	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = VulkanAPI::FindMemoryType(
								   memReqs.memoryTypeBits,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	ASSERT_VULKAN(vkAllocateMemory(device, &memAllocInfo, nullptr, &m_GLImagesMemory));
	ASSERT_VULKAN(vkBindImageMemory(device, m_GLImage, m_GLImagesMemory, 0));


	VkImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.image = m_GLImage;
	imageViewCreateInfo.flags = 0;
	imageViewCreateInfo.format = m_ComputeImageFormat;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.layerCount = CUBE_FACES;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_CubeImageView));

	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	// imageViewCreateInfo.subresourceRange.layerCount = CUBE_FACES;
	ASSERT_VULKAN(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ImageView));

	VkImageMemoryBarrier imageMemoryBarrier;
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.image = m_GLImage;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE_KHR;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // TODO: manage for later usage
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.layerCount = CUBE_FACES;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;

	VkCommandBufferBeginInfo beginInfo {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr
	};

	ASSERT_VULKAN(vkBeginCommandBuffer(m_LayoutCommandBuffer, &beginInfo));

	vkCmdPipelineBarrier(
		m_LayoutCommandBuffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);

	ASSERT_VULKAN(vkEndCommandBuffer(m_LayoutCommandBuffer));

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_LayoutCommandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
	// wait for completion.
	ASSERT_VULKAN(vkQueueWaitIdle(VulkanAPI::GetComputeQueue()));
}

void GroundLighting::CreateDescriptors(VkDevice device) {
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
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		// sample in fragment.
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	});

	VkDescriptorSetLayoutBinding imageBindings[IMAGE_DESC_COUNT];

	std::fill_n(imageBindings, IMAGE_DESC_COUNT, VkDescriptorSetLayoutBinding{
		.binding = 0,
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
	layoutCreateInfo.bindingCount = 1;

	layoutCreateInfo.pBindings = sampleBindings;
	ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_SampleDescriptorLayout));

	layoutCreateInfo.pBindings = imageBindings;
	ASSERT_VULKAN(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_ImageDescriptorLayout));

	VkSamplerCreateInfo sampler;
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.pNext = nullptr;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	// shouldn't address outside of edge, but to be safe.
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
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

	m_CubemapSampleDescriptor = allocTarget[0];
	m_CubemapImageDescriptor = allocTarget[1];

	std::vector<VkWriteDescriptorSet> writeDescSets(IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT);
	std::fill_n(writeDescSets.begin(), IMAGE_DESC_COUNT + SAMPLE_DESC_COUNT, VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr,
	});

	std::vector<VkDescriptorImageInfo> texInfo(IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT);
	std::fill_n(texInfo.begin(), IMAGE_DESC_COUNT+SAMPLE_DESC_COUNT, VkDescriptorImageInfo{
		.sampler = m_LinearSampler,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	});

	texInfo[0].imageView = m_ImageView,
	writeDescSets[0].pImageInfo = &texInfo[0];
	writeDescSets[0].dstSet = m_CubemapImageDescriptor;
	writeDescSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	texInfo[1].imageView = m_CubeImageView,
	writeDescSets[1].pImageInfo = &texInfo[1];
	writeDescSets[1].dstSet = m_CubemapSampleDescriptor;
	writeDescSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	vkUpdateDescriptorSets(device, writeDescSets.size(), writeDescSets.data(), 0, nullptr);
}

void GroundLighting::CreateCommandBuffers() {
	// one buffer for compute, one for transitioning the image.
	m_CommandPool.AllocateBuffers(1+1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	std::vector tmp(m_CommandPool.GetBuffers());
	m_ComputeCommandBuffer = tmp[0];
	m_LayoutCommandBuffer = tmp[1];
}

void GroundLighting::CreateComputePipeline(VkDevice device) {
	std::vector<VkDescriptorSetLayout> apLayouts(7);
	apLayouts[GRL_SETS_SUN] = m_Sun.GetDescriptorSetLayout();
	apLayouts[GRL_SETS_RATIO] = m_Precomp.GetRatioDescriptorSetLayout();
	apLayouts[GRL_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSetLayout(); 
	apLayouts[GRL_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSetLayout(); 
	apLayouts[GRL_SETS_SCATTERING_SAMPLER0] =  m_Atmosphere.GetScatteringSampleDescriptorLayout();
	apLayouts[GRL_SETS_SCATTERING_SAMPLER1] = m_Atmosphere.GetScatteringSampleDescriptorLayout();
	apLayouts[GRL_SETS_CUBEMAP_IMAGE] = m_ImageDescriptorLayout;

	VkPipelineLayoutCreateInfo apLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.setLayoutCount = static_cast<uint32_t>(apLayouts.size()),
		.pSetLayouts = apLayouts.data(),
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};
	ASSERT_VULKAN(vkCreatePipelineLayout(device, &apLayoutCreateInfo, nullptr, &m_GLPipelineLayout));

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
	pipeline.layout = m_GLPipelineLayout;

	ASSERT_VULKAN(vkCreateComputePipelines(device, nullptr, 1, &pipeline, nullptr, &m_GLPipeline));
}

void GroundLighting::RecordCommandBuffers() {
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	ASSERT_VULKAN(vkBeginCommandBuffer(m_ComputeCommandBuffer, &beginInfo));

	vkCmdBindPipeline(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_GLPipeline);

	std::vector<VkDescriptorSet> sets(7);
	sets[GRL_SETS_SUN] = m_Sun.GetDescriptorSet();
	sets[GRL_SETS_RATIO] = m_Precomp.GetRatioDescriptorSet();
	sets[GRL_SETS_ENV0] = m_Precomp.GetEffectiveEnv(0).GetDescriptorSet(); 
	sets[GRL_SETS_ENV1] = m_Precomp.GetEffectiveEnv(1).GetDescriptorSet(); 
	sets[GRL_SETS_SCATTERING_SAMPLER0] =  m_Atmosphere.GetScatteringSampleDescriptorSet(0);
	sets[GRL_SETS_SCATTERING_SAMPLER1] = m_Atmosphere.GetScatteringSampleDescriptorSet(1);
	sets[GRL_SETS_CUBEMAP_IMAGE] = m_CubemapImageDescriptor;

	vkCmdBindDescriptorSets(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_GLPipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

	// X*Y per layer.
	vkCmdDispatch(m_ComputeCommandBuffer, GRL_X, GRL_Y, CUBE_FACES);

	vkEndCommandBuffer(m_ComputeCommandBuffer);
}

void GroundLighting::Compute() {
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = 0;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_ComputeCommandBuffer;

	ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));
}

VkDescriptorSet GroundLighting::GetSampleDescriptorSet() const {
	return m_CubemapSampleDescriptor;
}

VkDescriptorSetLayout GroundLighting::GetSampleDescriptorLayout() const {
	return m_SampleDescriptorLayout;
}

} // namespace en
