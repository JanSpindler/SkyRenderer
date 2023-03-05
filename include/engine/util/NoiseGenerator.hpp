#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/vulkan/ComputePipeline.hpp>

namespace en
{
	typedef std::vector<std::vector<float>> VecVecF;
	typedef std::vector<VecVecF> VecVecVecF;

	class NoiseGenerator
	{
	public:
		static void Init();
		static void Shutdown();

		static VecVecF Perlin2D(const glm::uvec2& size, const glm::vec2& seed, float freq, float min, float max);
		static VecVecF Worley2D(const glm::uvec2& size, uint32_t cubeSideLength, float min, float max);
		static VecVecF NoNoise2D(const glm::uvec2& size, float value);
		static VecVecF FitInRange2D(const VecVecF& values, float targetMin, float targetMax);

		static VecVecVecF Perlin3D(const glm::uvec3& size, const glm::vec3& seed, float freq, float min, float max);
		static VecVecVecF Worley3D(const glm::uvec3& size, uint32_t cubeSideLength, bool vulkanCompute, float min, float max);
		static VecVecVecF NoNoise3D(const glm::uvec3& size, float value);
		static void FitInRange3D(VecVecVecF& values, float targetMin, float targetMax);

	private:
		static vk::Shader* m_WorleyShader;
		static vk::ComputePipeline* m_ComputePipeline;
	};
}
