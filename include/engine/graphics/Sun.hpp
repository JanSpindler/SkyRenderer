#pragma once

#include "engine/graphics/vulkan/Buffer.hpp"
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

// properly aligned by default!
struct SunData {
	glm::vec3 m_Color;
	float m_Zenith;
	glm::vec3 m_SunDir;
	float m_Azimuth;
};

namespace en {
	class Sun {
		public:
			static void Init();
			static void Destroy();

			Sun(float zenith, float azimuth, glm::vec3 color);
			~Sun();

			void SetZenith(float z);
			void SetAzimuth(float a);
			void SetColor(glm::vec3 c);

			float GetZenith() const;

			VkDescriptorSetLayout GetDescriptorSetLayout() const;
			VkDescriptorSet GetDescriptorSet() const;

			void RenderImgui();

		private:
			static VkDescriptorSetLayout m_DescriptorSetLayout;
			static VkDescriptorPool m_Pool;

			SunData m_SunData; 
			VkDescriptorSet m_DescriptorSet;
			vk::Buffer m_UniformBuffer;

			void UpdateBuffer();
	};
}
