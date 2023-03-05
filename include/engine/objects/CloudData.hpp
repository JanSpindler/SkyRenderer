#pragma once

#include <engine/graphics/vulkan/Texture3D.hpp>
#include <engine/graphics/vulkan/Texture2D.hpp>
#include <engine/graphics/vulkan/Buffer.hpp>
#include <glm/glm.hpp>

namespace en
{
	const uint32_t MAX_CLOUD_DATA_COUNT = 16;

	struct CloudSampleCounts
	{
		int primary;
		int secondary;
	};

	struct CloudUniformData
	{
		glm::vec3 skySize;
		float _______________;
		glm::vec3 skyPos;
		float shapeScale;
		float detailThreshold;
		float detailFactor;
		float detailScale;
		float heightGradientMinVal;
		float heightGradientMaxVal;
		float g;
		float ambientGradientMinVal;
		float ambientGradientMaxVal;
		float jitterStrength;
		float sigmaS;
		float sigmaE;
		float temporalUpsampling;
		uint32_t ltee;
		uint32_t htee;

		bool operator==(const CloudUniformData& other);
		bool operator!=(const CloudUniformData& other);
	};

	class CloudData
	{
	public:
		static void Init();
		static void Shutdown();
		static VkDescriptorSetLayout GetDescriptorSetLayout();

		CloudData();

		void Destroy();
		
		void RenderImGui();

		VkDescriptorSet GetDescriptorSet() const;
		const CloudSampleCounts& GetSampleCounts() const;
		bool HaveSampleCountsChanged() const;

	private:
		static VkDescriptorSetLayout m_DescriptorSetLayout;
		static VkDescriptorPool m_DescriptorPool;

		vk::Texture3D m_CloudShapeTexture;
		vk::Texture3D m_CloudDetailTexture;
		vk::Texture2D m_WeatherTexture;
		CloudUniformData m_UniformData;
		vk::Buffer* m_UniformBuffer;
		VkDescriptorSet m_DescriptorSet;

		CloudSampleCounts m_SampleCounts;
		bool m_SampleCountsChanged;
	};
}
