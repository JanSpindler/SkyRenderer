#pragma once

#include "engine/graphics/vulkan/Buffer.hpp"
#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <vulkan/vulkan_core.h>

namespace en {
	class EnvConditions {
		public:
			struct Environment {
				glm::vec3 m_OzoneExtinctionCoefficient;
				float m_PlanetRadius;
				float m_AtmosphereHeight;
				float m_RefractiveIndexAir;
				float m_AirDensityAtSeaLevel;
				float m_MieScatteringCoefficient;
				float m_AsymmetryFactor;
				float m_RayleighScaleHeight;
				float m_MieScaleHeight;
			};

			EnvConditions(Environment conds);
			EnvConditions(EnvConditions &conds);
			~EnvConditions();

			void RenderImgui();
			VkDescriptorSet GetDescriptorSet() const;
			VkDescriptorSetLayout GetDescriptorSetLayout() const;
			Environment GetEnvironment();
			void SetEnvironment(const Environment &env);

		private:
			// All variables aligned by default.
			struct EnvironmentData {
				glm::vec3 m_RayleighSCoeff;
				float m_PlanetRadius;
				glm::vec3 m_MieSCoeff;
				float m_AtmosphereRadius;
				glm::vec3 m_OzoneExtinctionCoefficient;
				float m_AtmosphereHeight;
				float m_AsymmetryFactor;
				float m_RayleighScaleHeight;
				float m_MieScaleHeight;

				EnvironmentData(Environment);
			};

			Environment m_Env;

			EnvironmentData m_EnvData;

			VkDescriptorPool m_DescriptorPool;
			VkDescriptorSetLayout m_DescriptorSetLayout;
			VkDescriptorSet m_DescriptorSet;

			vk::Buffer m_EnvUBO;
	};
}
