#pragma once
#include <engine/objects/Model.hpp>
namespace en
{
	class Terrain : public Model
	{
	public:
		Terrain(
			uint32_t sideVertexCount,
			float vertexSpacing,
			float baseFreq,
			float amplitude,
			float exponent,
			float zeroHeightRadius,
			const glm::vec2& seed);

	private:
		void GenerateMesh(const std::vector<std::vector<glm::vec3>>& heightMap);
		std::vector<std::vector<glm::vec3>> GenerateHeightMap(
			uint32_t sideVertexCount,
			float vertexSpacing,
			float baseFreq,
			float amplitude,
			float exponent,
			float zeroHeightRadius,
			const glm::vec2& seed) const;
		float RandomHeight(const glm::vec2& pos, float baseFreq, float exponent, const glm::vec2& seed) const;
	};
}
