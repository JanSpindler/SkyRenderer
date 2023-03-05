#pragma once

#include <engine/graphics/vulkan/Buffer.hpp>
#include <glm/glm.hpp>

namespace en
{
	struct WindUniformData
	{
		glm::vec2 offset;
	};

	class Wind
	{
	public:
		static const uint32_t MAX_COUNT = 16;

		static void Init();
		static void Shutdown();
		static VkDescriptorSetLayout GetDescriptorSetLayout();

		Wind(float angle, float strength);

		void Destroy();

		void Update(float deltaTime);

		VkDescriptorSet GetDescriptorSet() const;

	private:
		static VkDescriptorSetLayout m_DescriptorSetLayout;
		static VkDescriptorPool m_DescriptorPool;

		float m_Angle;
		float m_Strength;

		WindUniformData m_UniformData;
		vk::Buffer m_UniformBuffer;
		VkDescriptorSet m_DescriptorSet;
	};
}
