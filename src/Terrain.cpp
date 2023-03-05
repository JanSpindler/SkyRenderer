#include <engine/objects/Terrain.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/noise.hpp>
namespace en
{
	Terrain::Terrain(
		uint32_t sideVertexCount,
		float vertexSpacing,
		float baseFreq,
		float amplitude,
		float exponent,
		float zeroHeightRadius,
		const glm::vec2& seed)
	{
		m_Materials.push_back(new Material(glm::vec4(glm::vec3(0.6f), 1.0f), nullptr));

		GenerateMesh(
			GenerateHeightMap(
				sideVertexCount,
				vertexSpacing,
				baseFreq,
				amplitude,
				exponent,
				zeroHeightRadius,
				seed));
	}

	void Terrain::GenerateMesh(const std::vector<std::vector<glm::vec3>>& heightMap)
	{
		uint32_t sideVertexCount = heightMap.size();

		// Create vertex array
		std::vector<PNTVertex> vertices;
		for (uint32_t x = 0; x < sideVertexCount; x++)
		{
			for (uint32_t z = 0; z < sideVertexCount; z++)
			{
				glm::vec3 pos = heightMap[x][z];
				glm::vec3 normal(0.0f, 1.0f, 0.0f);
				if (x < sideVertexCount - 1 && z < sideVertexCount - 1)
				{
					// Calculate normal if possible
					glm::vec3 dx = heightMap[x + 1][z] - pos;
					glm::vec3 dz = heightMap[x][z + 1] - pos;
					normal = -glm::normalize(glm::cross(dx, dz));
				}
				glm::vec2 uv(0.0f, 0.0f);
				vertices.emplace_back(pos, normal, uv);
			}
		}

		// Create index array
		std::vector<uint32_t> indices;
		for (uint32_t x = 0; x < sideVertexCount - 1; x++)
		{
			for (uint32_t z = 0; z < sideVertexCount - 1; z++)
			{
				uint32_t i0 = x * sideVertexCount + z;
				uint32_t i1 = (x + 1) * sideVertexCount + z;
				uint32_t i2 = (x + 1) * sideVertexCount + (z + 1);
				uint32_t i3 = x * sideVertexCount + (z + 1);
				std::vector<uint32_t> quadIndices = { i0, i3, i2, i2, i1, i0 };
				indices.insert(indices.end(), quadIndices.begin(), quadIndices.end());
			}
		}

		// Create mesh
		m_Meshes.push_back(new Mesh(vertices, indices, m_Materials[0]));
	}

	std::vector<std::vector<glm::vec3>> Terrain::GenerateHeightMap(
		uint32_t sideVertexCount,
		float vertexSpacing,
		float baseFreq,
		float amplitude,
		float exponent,
		float zeroHeightRadius,
		const glm::vec2& seed) const
	{
		// Allocate height map
		std::vector<std::vector<glm::vec3>> heightMap(sideVertexCount);
		for (std::vector<glm::vec3>& vf : heightMap)
			vf.resize(sideVertexCount);

		// Fill height map
		uint32_t offset = sideVertexCount / 2;
		for (uint32_t x = 0; x < sideVertexCount; x++)
		{
			for (uint32_t z = 0; z < sideVertexCount; z++)
			{
				float offset = static_cast<float>(sideVertexCount) / 2.0f;
				float pX = static_cast<float>(x) - offset;
				float pZ = static_cast<float>(z) - offset;
				
				float pY = 0.0f;
				if (glm::length(glm::vec2(pX, pZ)) > zeroHeightRadius)
					pY = RandomHeight(glm::vec2(pX, pZ), baseFreq, exponent, seed) * amplitude;
					
				heightMap[x][z] = glm::vec3(pX * vertexSpacing, pY, pZ * vertexSpacing);
			}
		}

		return heightMap;
	}

	float Terrain::RandomHeight(const glm::vec2& pos, float baseFreq, float exponent, const glm::vec2& seed) const
	{
		const float persistance = 0.5f;
		const float lacunarity = 2.0f;
		const uint32_t octaveCount = 16;

		float freq = baseFreq;
		float ampl = 1.0f;
		float height = 0.0f;
		float norm = 0.0f;
		for (uint32_t i = 0; i < octaveCount; i++)
		{
			float perlin = glm::perlin(pos * freq + seed) + 0.5f;
			height += perlin * ampl;
			norm += ampl;
			ampl *= persistance;
			freq *= lacunarity;
		}
		height /= norm;
		height = glm::pow(height, exponent);
		height -= 0.5f;
		//Log::Info(std::to_string(height));
		return height;
	}
}
