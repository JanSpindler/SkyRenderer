#include <engine/objects/CloudData.hpp>
#include <engine/util/NoiseGenerator.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <engine/util/ReadFile.hpp>

namespace en
{
	bool CloudUniformData::operator==(const CloudUniformData& other)
	{
		return
			this->skySize == other.skySize &&
			this->skyPos == other.skyPos &&
			this->shapeScale == other.shapeScale &&
			this->detailThreshold == other.detailThreshold &&
			this->detailFactor == other.detailFactor &&
			this->detailScale == other.detailScale &&
			this->heightGradientMinVal == other.heightGradientMinVal &&
			this->heightGradientMaxVal == other.heightGradientMaxVal &&
			this->g == other.g &&
			this->ambientGradientMinVal == other.ambientGradientMinVal &&
			this->ambientGradientMaxVal == other.ambientGradientMaxVal &&
			this->jitterStrength == other.jitterStrength &&
			this->sigmaS == other.sigmaS &&
			this->sigmaE == other.sigmaE &&
			this->temporalUpsampling == other.temporalUpsampling &&
			this->ltee == other.ltee &&
			this->htee == other.htee;
	}

	bool CloudUniformData::operator!=(const CloudUniformData& other)
	{
		return !operator==(other);
	}

	VkDescriptorSetLayout CloudData::m_DescriptorSetLayout;
	VkDescriptorPool CloudData::m_DescriptorPool;

	void CloudData::Init()
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding dataBinding;
		dataBinding.binding = 0;
		dataBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dataBinding.descriptorCount = 1;
		dataBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		dataBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding cloudShapeBinding;
		cloudShapeBinding.binding = 1;
		cloudShapeBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudShapeBinding.descriptorCount = 1;
		cloudShapeBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		cloudShapeBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding cloudDetailBinding;
		cloudDetailBinding.binding = 2;
		cloudDetailBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudDetailBinding.descriptorCount = 1;
		cloudDetailBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		cloudDetailBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding weatherBinding;
		weatherBinding.binding = 3;
		weatherBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		weatherBinding.descriptorCount = 1;
		weatherBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		weatherBinding.pImmutableSamplers = nullptr;

		std::vector<VkDescriptorSetLayoutBinding> bindings = { dataBinding, cloudShapeBinding, cloudDetailBinding, weatherBinding };

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.pNext = nullptr;
		descSetLayoutCreateInfo.flags = 0;
		descSetLayoutCreateInfo.bindingCount = bindings.size();
		descSetLayoutCreateInfo.pBindings = bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create Descriptor Pool
		VkDescriptorPoolSize dataSize;
		dataSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dataSize.descriptorCount = 1;

		VkDescriptorPoolSize cloudShapeSize;
		cloudShapeSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudShapeSize.descriptorCount = 1;

		VkDescriptorPoolSize cloudDetailSize;
		cloudDetailSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudDetailSize.descriptorCount = 1;

		VkDescriptorPoolSize weatherSize;
		weatherSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		weatherSize.descriptorCount = 1;

		std::vector<VkDescriptorPoolSize> poolSizes = { dataSize, cloudShapeSize, cloudDetailSize, weatherSize };

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = MAX_CLOUD_DATA_COUNT;
		descPoolCreateInfo.poolSizeCount = poolSizes.size();
		descPoolCreateInfo.pPoolSizes = poolSizes.data();

		result = vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool);
		ASSERT_VULKAN(result);
	}

	void CloudData::Shutdown()
	{
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}
	
	VkDescriptorSetLayout CloudData::GetDescriptorSetLayout()
	{
		return m_DescriptorSetLayout;
	}

	CloudData::CloudData() :
		m_CloudShapeTexture(
			{	NoiseGenerator::Perlin3D(glm::uvec3(128, 32, 128), glm::vec3(0.0f), 1.0f / 48.0f, 0.0f, 1.0f),
				NoiseGenerator::Worley3D(glm::uvec3(128, 32, 128), 16, true, 0.0f, 1.0f),
				NoiseGenerator::Worley3D(glm::uvec3(128, 32, 128), 8, true, 0.0f, 1.0f),
				NoiseGenerator::Worley3D(glm::uvec3(128, 32, 128), 4, true, 0.0f, 1.0f) },
			VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		m_CloudDetailTexture(
			{	NoiseGenerator::Worley3D(glm::uvec3(32), 8, true, 0.0f, 1.0f),
				NoiseGenerator::Worley3D(glm::uvec3(32), 4, true, 0.0f, 1.0f),
				NoiseGenerator::Worley3D(glm::uvec3(32), 2, true, 0.0f, 1.0f),
				NoiseGenerator::NoNoise3D(glm::uvec3(32), 1.0f) },
			VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		m_WeatherTexture(
			{	NoiseGenerator::Worley2D(glm::uvec2(256), 16, -0.1f, 1.0f),
				NoiseGenerator::Perlin2D(glm::uvec2(256), glm::vec2(20.0f, 10.0f), 1.0f / 16.0f, 0.4f, 0.7f),
				NoiseGenerator::Perlin2D(glm::uvec2(256), glm::vec2(10.0f, 30.0f), 1.0f / 16.0f, 0.0f, 0.3f),
				NoiseGenerator::NoNoise2D(glm::uvec2(256), 1.0f) },
			VK_FILTER_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		m_UniformBuffer(new vk::Buffer(
			sizeof(CloudUniformData), 
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			{})),
		m_SampleCounts({ 48, 4 }),
		m_SampleCountsChanged(false)
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Uniform data
		m_UniformData.skySize = glm::vec3(2048.0f, 128.0f, 2048.0f);
		m_UniformData.skyPos = glm::vec3(0.0f, 0.0f, 0.0f);
		m_UniformData.shapeScale = 0.5f;
		m_UniformData.detailThreshold = 0.15f;
		m_UniformData.detailFactor = 0.2f;
		m_UniformData.detailScale = 3.0f;
		m_UniformData.heightGradientMinVal = 0.6f;
		m_UniformData.heightGradientMaxVal = 1.0f;
		m_UniformData.g = 0.75f;
		m_UniformData.ambientGradientMinVal = 0.6f;
		m_UniformData.ambientGradientMaxVal = 1.0f;
		m_UniformData.jitterStrength = 1.0f;
		m_UniformData.sigmaS = 0.35f;
		m_UniformData.sigmaE = 0.35f;
		m_UniformData.temporalUpsampling = 0.05f;
		m_UniformData.ltee = 1;
		m_UniformData.htee = 0;

		m_UniformBuffer->MapMemory(sizeof(CloudUniformData), &m_UniformData, 0, 0);

		// Allocate descriptor set
		VkDescriptorSetAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.descriptorPool = m_DescriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &m_DescriptorSetLayout;

		VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &m_DescriptorSet);
		ASSERT_VULKAN(result);

		// Write descriptor set
		VkDescriptorBufferInfo dataBufferInfo;
		dataBufferInfo.buffer = m_UniformBuffer->GetVulkanHandle();
		dataBufferInfo.offset = 0;
		dataBufferInfo.range = sizeof(CloudUniformData);

		VkWriteDescriptorSet dataWrite;
		dataWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		dataWrite.pNext = nullptr;
		dataWrite.dstSet = m_DescriptorSet;
		dataWrite.dstBinding = 0;
		dataWrite.dstArrayElement = 0;
		dataWrite.descriptorCount = 1;
		dataWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dataWrite.pImageInfo = nullptr;
		dataWrite.pBufferInfo = &dataBufferInfo;
		dataWrite.pTexelBufferView = nullptr;

		VkDescriptorImageInfo cloudShapeImageInfo;
		cloudShapeImageInfo.sampler = m_CloudShapeTexture.GetSampler();
		cloudShapeImageInfo.imageView = m_CloudShapeTexture.GetImageView();
		cloudShapeImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet cloudShapeWrite;
		cloudShapeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cloudShapeWrite.pNext = nullptr;
		cloudShapeWrite.dstSet = m_DescriptorSet;
		cloudShapeWrite.dstBinding = 1;
		cloudShapeWrite.dstArrayElement = 0;
		cloudShapeWrite.descriptorCount = 1;
		cloudShapeWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudShapeWrite.pImageInfo = &cloudShapeImageInfo;
		cloudShapeWrite.pBufferInfo = nullptr;
		cloudShapeWrite.pTexelBufferView = nullptr;

		VkDescriptorImageInfo cloudDetailImageInfo;
		cloudDetailImageInfo.sampler = m_CloudDetailTexture.GetSampler();
		cloudDetailImageInfo.imageView = m_CloudDetailTexture.GetImageView();
		cloudDetailImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet cloudDetailWrite;
		cloudDetailWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cloudDetailWrite.pNext = nullptr;
		cloudDetailWrite.dstSet = m_DescriptorSet;
		cloudDetailWrite.dstBinding = 2;
		cloudDetailWrite.dstArrayElement = 0;
		cloudDetailWrite.descriptorCount = 1;
		cloudDetailWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cloudDetailWrite.pImageInfo = &cloudDetailImageInfo;
		cloudDetailWrite.pBufferInfo = nullptr;
		cloudDetailWrite.pTexelBufferView = nullptr;

		VkDescriptorImageInfo weatherImageInfo;
		weatherImageInfo.sampler = m_WeatherTexture.GetSampler();
		weatherImageInfo.imageView = m_WeatherTexture.GetImageView();
		weatherImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet weatherWrite;
		weatherWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		weatherWrite.pNext = nullptr;
		weatherWrite.dstSet = m_DescriptorSet;
		weatherWrite.dstBinding = 3;
		weatherWrite.dstArrayElement = 0;
		weatherWrite.descriptorCount = 1;
		weatherWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		weatherWrite.pImageInfo = &weatherImageInfo;
		weatherWrite.pBufferInfo = nullptr;
		weatherWrite.pTexelBufferView = nullptr;

		std::vector<VkWriteDescriptorSet> descWrites = { dataWrite, cloudShapeWrite, cloudDetailWrite, weatherWrite };

		vkUpdateDescriptorSets(device, descWrites.size(), descWrites.data(), 0, nullptr);
	}

	void CloudData::Destroy()
	{
		m_UniformBuffer->Destroy();
		delete m_UniformBuffer;

		m_WeatherTexture.Destroy();
		m_CloudDetailTexture.Destroy();
		m_CloudShapeTexture.Destroy();
	}

	void CloudData::RenderImGui()
	{
		// Copy old data
		CloudUniformData oldData = m_UniformData;
		CloudSampleCounts oldSampleCount = m_SampleCounts;

		// Imgui
		ImGui::Begin("Cloud Data");
		ImGui::SliderFloat3("Sky Size", glm::value_ptr(m_UniformData.skySize), 0.0f, 1024.0f);
		ImGui::SliderFloat3("Sky Pos", glm::value_ptr(m_UniformData.skyPos), -512.0f, 512.0f);
		ImGui::SliderFloat("Shape Scale", &m_UniformData.shapeScale, 0.0f, 8.0f);
		ImGui::SliderFloat("Detail Threshold", &m_UniformData.detailThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Detail Factor", &m_UniformData.detailFactor, 0.0f, 1.0f);
		ImGui::SliderFloat("Detail Scale", &m_UniformData.detailScale, 0.0f, 8.0f);
		ImGui::SliderFloat("Height Gradient Min Val", &m_UniformData.heightGradientMinVal, 0.0f, 1.0f);
		ImGui::SliderFloat("Height Gradient Max Val", &m_UniformData.heightGradientMaxVal, 0.0f, 1.0f);
		ImGui::SliderFloat("G", &m_UniformData.g, 0.0f, 1.0f);
		ImGui::SliderFloat("Ambient Gradient Min Val", &m_UniformData.ambientGradientMinVal, 0.0f, 1.0f);
		ImGui::SliderFloat("Ambient Gradient Max Val", &m_UniformData.ambientGradientMaxVal, 0.0f, 1.0f);
		ImGui::SliderFloat("Jitter Strength", &m_UniformData.jitterStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("Sigma S", &m_UniformData.sigmaS, 0.0f, 4.0f);
		ImGui::SliderFloat("Sigma E", &m_UniformData.sigmaE, 0.0f, 4.0f);
		ImGui::SliderInt("Primary Sample Count", &m_SampleCounts.primary, 1, 128);
		ImGui::SliderInt("Secondary Sample Count", &m_SampleCounts.secondary, 0, 8);
		ImGui::SliderFloat("Temporal Upsampling", &m_UniformData.temporalUpsampling, 0.0f, 1.0f);
		ImGui::Checkbox("LTEE", reinterpret_cast<bool*>(&m_UniformData.ltee));
		ImGui::Checkbox("HTEE", reinterpret_cast<bool*>(&m_UniformData.htee));
		ImGui::End();

		// Check if uniform data changed
		if (oldData != m_UniformData)
		{
			// Update uniform buffer
			m_UniformBuffer->MapMemory(sizeof(CloudUniformData), &m_UniformData, 0, 0);
		}

		// Check if sample counts changed
		m_SampleCountsChanged =
			oldSampleCount.primary != m_SampleCounts.primary ||
			oldSampleCount.secondary != m_SampleCounts.secondary;
	}

	VkDescriptorSet CloudData::GetDescriptorSet() const
	{
		return m_DescriptorSet;
	}

	const CloudSampleCounts& CloudData::GetSampleCounts() const
	{
		return m_SampleCounts;
	}

	bool CloudData::HaveSampleCountsChanged() const
	{
		return m_SampleCountsChanged;
	}
}
