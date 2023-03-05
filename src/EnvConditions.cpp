#include <imgui.h>
#include <vulkan/vulkan_core.h>
#define _USE_MATH_DEFINES
#include <cmath>

#include <glm/gtx/string_cast.hpp>

#include <engine/graphics/EnvConditions.hpp>

const double red_wavelength   {0.00000065};
const double green_wavelength {0.00000051};
const double blue_wavelength  {0.000000475};

// 3.2.2
float rayleigh_scattering_coefficient(double n, double N, double lambda) {
	return static_cast<float>(8*M_PI*M_PI*M_PI*(n*n-1)*(n*n-1)/(3*N*lambda*lambda*lambda*lambda));
}

namespace en {
	EnvConditions::EnvironmentData::EnvironmentData(EnvConditions::Environment conds) :
		// independent of wavelength.
		m_MieSCoeff{glm::vec3(conds.m_MieScatteringCoefficient)},
		m_RayleighScaleHeight{conds.m_RayleighScaleHeight},
		m_MieScaleHeight{conds.m_MieScaleHeight},
		m_AtmosphereHeight{conds.m_AtmosphereHeight},
		m_PlanetRadius{conds.m_PlanetRadius},
		m_AtmosphereRadius{static_cast<float>(conds.m_AtmosphereHeight+conds.m_PlanetRadius)},
		m_RayleighSCoeff{
			rayleigh_scattering_coefficient(conds.m_RefractiveIndexAir, conds.m_AirDensityAtSeaLevel, red_wavelength),
			rayleigh_scattering_coefficient(conds.m_RefractiveIndexAir, conds.m_AirDensityAtSeaLevel, green_wavelength),
			rayleigh_scattering_coefficient(conds.m_RefractiveIndexAir, conds.m_AirDensityAtSeaLevel, blue_wavelength)},
		m_AsymmetryFactor(conds.m_AsymmetryFactor) { }

	EnvConditions::EnvConditions(EnvConditions::Environment conds) :
		m_Env{conds},
		m_EnvData(conds),
		m_EnvUBO(
			sizeof(EnvironmentData),
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			{}) {
		VkDevice device = VulkanAPI::GetDevice();

		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding layoutBinding;
		layoutBinding.binding = 0;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.pNext = nullptr;
		descSetLayoutCreateInfo.flags = 0;
		descSetLayoutCreateInfo.bindingCount = 1;
		descSetLayoutCreateInfo.pBindings = &layoutBinding;

		VkResult result = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create Descriptor Pool
		VkDescriptorPoolSize poolSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = 1;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		// Always create new pool, not the most efficient way but ok.
		result = vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool);
		ASSERT_VULKAN(result);

		// Allocate Descriptor Set
		VkDescriptorSetAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.descriptorPool = m_DescriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &m_DescriptorSetLayout;

		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocateInfo, &m_DescriptorSet));

		// Write Descriptor Set
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = m_EnvUBO.GetVulkanHandle();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(EnvironmentData);

		VkWriteDescriptorSet writeDescSet;
		writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescSet.pNext = nullptr;
		writeDescSet.dstSet = m_DescriptorSet;
		writeDescSet.dstBinding = 0;
		writeDescSet.dstArrayElement = 0;
		writeDescSet.descriptorCount = 1;
		writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescSet.pImageInfo = nullptr;
		writeDescSet.pBufferInfo = &bufferInfo;
		writeDescSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
		m_EnvUBO.MapMemory(sizeof(EnvironmentData), &m_EnvData, 0, 0);
	}

	// create new object from same Environment. (creates a copy).
	EnvConditions::EnvConditions(EnvConditions &conds) : EnvConditions(conds.m_Env){ }

	EnvConditions::~EnvConditions() {
		VkDevice device = en::VulkanAPI::GetDevice();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		m_EnvUBO.Destroy();
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}

	void EnvConditions::RenderImgui() {
		ImGui::Begin("Env");
		if (ImGui::SliderFloat("PlanetRadius", &m_Env.m_PlanetRadius, 2000000, 10000000) |
			ImGui::SliderFloat("AtmosphereHeight", &m_Env.m_AtmosphereHeight, 10000, 1000000) |
			ImGui::SliderFloat("RefractiveIndexAir", &m_Env.m_RefractiveIndexAir, 1.00001, 1.001, "%6f") |
			ImGui::SliderFloat("AirDensityAtSeaLevel", &m_Env.m_AirDensityAtSeaLevel, pow(10, 23), pow(10, 27)) |
			ImGui::SliderFloat("MieScatteringCoefficient", &m_Env.m_MieScatteringCoefficient, 0.0000001, 0.00001, "%8f") |
			// Mie should scatter forwards.
			ImGui::SliderFloat("AsymmetryFactor", &m_Env.m_AsymmetryFactor, -0.01, -0.99) |
			ImGui::SliderFloat("RayleighScaleHeight", &m_Env.m_RayleighScaleHeight, 10, m_Env.m_AtmosphereHeight) |
			ImGui::SliderFloat("MieScaleHeight", &m_Env.m_MieScaleHeight, 10, m_Env.m_AtmosphereHeight)) {

			// update UBO.
			m_EnvData = EnvironmentData(m_Env);
			m_EnvUBO.MapMemory(sizeof(EnvironmentData), &m_EnvData, 0, 0);
		}

			
		ImGui::End();
	}

	void EnvConditions::SetEnvironment(const EnvConditions::Environment &env) {
		m_Env = env;
		m_EnvData = EnvironmentData(m_Env);
		m_EnvUBO.MapMemory(sizeof(EnvironmentData), &m_EnvData, 0, 0);
	}

	VkDescriptorSet EnvConditions::GetDescriptorSet() {
		return m_DescriptorSet;
	}

	VkDescriptorSetLayout EnvConditions::GetDescriptorSetLayout() {
		return m_DescriptorSetLayout;
	}

	EnvConditions::Environment EnvConditions::GetEnvironment() {
		return m_Env;
	}
}
