#include <engine/util/NoiseGenerator.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
#include <limits>
#include <engine/util/Log.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <engine/graphics/vulkan/Buffer.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>

namespace en
{
    vk::Shader* NoiseGenerator::m_WorleyShader;
    vk::ComputePipeline* NoiseGenerator::m_ComputePipeline;

    void NoiseGenerator::Init()
    {
        m_WorleyShader = new vk::Shader("compute/worley3d.comp", false);
        m_ComputePipeline = new vk::ComputePipeline(m_WorleyShader);
    }

    void NoiseGenerator::Shutdown()
    {
        m_ComputePipeline->Destroy();
        delete m_ComputePipeline;

        m_WorleyShader->Destroy();
        delete m_WorleyShader;
    }

    VecVecF NoiseGenerator::Perlin2D(const glm::uvec2& size, const glm::vec2& seed, float freq, float min, float max)
    {
        // Allocate
        VecVecF values(size.x);
        for (std::vector<float>& vf : values)
            vf.resize(size.y);

        // Generate perlin values
        for (uint32_t i = 0; i < size.x; i++)
        {
            for (uint32_t j = 0; j < size.y; j++)
            {
                glm::vec2 pos = static_cast<glm::vec2>(glm::uvec2(i, j));
                glm::vec2 samplePos = pos * freq + seed;
                float value = glm::perlin(samplePos) * 0.5f + 0.5f;
                values[i][j] = value;
            }
        }

        // Normalize
        return FitInRange2D(values, min, max);
    }

    VecVecF NoiseGenerator::Worley2D(const glm::uvec2& size, uint32_t cubeSideLength, float min, float max)
    {
        // Allocate
        VecVecF values(size.x);
        for (std::vector<float>& vf : values)
        {
            vf.resize(size.y);
        }

        // Generate random point in cubes
        glm::uvec2 cubePointSize = size / cubeSideLength;
        std::vector<glm::vec2> cubePoints;
        for (uint32_t i = 0; i < cubePointSize.x; i++)
        {
            for (uint32_t j = 0; j < cubePointSize.y; j++)
            {
                glm::vec2 cubePoint = static_cast<glm::vec2>(glm::uvec2(i, j));
                cubePoint += glm::linearRand(glm::vec2(0.0f), glm::vec2(1.0f));
                cubePoint *= static_cast<float>(cubeSideLength);
                cubePoints.push_back(cubePoint);
            }
        }

        // Sample distances to closest cube points
        for (uint32_t i = 0; i < size.x; i++)
        {
            for (uint32_t j = 0; j < size.y; j++)
            {
                float minDistance = std::numeric_limits<float>::max();
                for (const glm::vec2& cubePoint : cubePoints) // TODO: optimize
                {
                    glm::vec2 currentPos = static_cast<glm::vec2>(glm::uvec2(i, j));
                    float distance = glm::length(cubePoint - currentPos);
                    if (distance < minDistance)
                        minDistance = distance;
                }
                values[i][j] = minDistance;
            }
        }

        // Normalize
        return FitInRange2D(values, min, max);
    }

    VecVecF NoiseGenerator::NoNoise2D(const glm::uvec2& size, float value)
    {
        // Allocate and fill
        VecVecF values(size.x);
        for (std::vector<float>& vf : values)
        {
            vf.resize(size.y);
            for (float& f : vf)
                f = value;
        }

        return values;
    }

    VecVecF NoiseGenerator::FitInRange2D(const VecVecF& inValues, float targetMin, float targetMax)
    {
        VecVecF outValues;
        outValues.resize(inValues.size());
        for (std::vector<float>& vf : outValues)
            vf.resize(inValues[0].size());

        // Calculate current min and max
        float currentMin = std::numeric_limits<float>::max();
        float currentMax = std::numeric_limits<float>::min();
        for (uint32_t x = 0; x < inValues.size(); x++)
        {
            for (uint32_t y = 0; y < inValues[x].size(); y++)
            {
                float val = inValues[x][y];
                if (val < currentMin)
                    currentMin = val;
                if (val > currentMax)
                    currentMax = val;
            }
        }

        // Fit in target range
        for (uint32_t x = 0; x < outValues.size(); x++)
        {
            for (uint32_t y = 0; y < outValues[x].size(); y++)
            {
                float val = inValues[x][y];
                float result = (val - currentMin) / (currentMax - currentMin); // In range [0, 1]
                result = result * (targetMax - targetMin) + targetMin; // In range [targetMin, targetMax]
                outValues[x][y] = result;
            }
        }

        return outValues;
    }

	VecVecVecF NoiseGenerator::Perlin3D(const glm::uvec3& size, const glm::vec3& seed, float freq, float min, float max)
	{
        // Allocate
        VecVecVecF values(size.x);
        for (std::vector<std::vector<float>>& vvf : values)
        {
            vvf.resize(size.y);
            for (std::vector<float>& vf : vvf)
                vf.resize(size.z);
        }

        // Generate perlin values
        for (uint32_t i = 0; i < size.x; i++)
        {
            for (uint32_t j = 0; j < size.y; j++)
            {
                for (uint32_t k = 0; k < size.z; k++)
                {
                    glm::vec3 pos = static_cast<glm::vec3>(glm::uvec3(i, j, k));
                    glm::vec3 samplePos = pos * freq + seed;
                    float value = glm::perlin(samplePos) * 0.5f + 0.5f;
                    values[i][j][k] = value;
                }
            }
        }

        // Normalize
        FitInRange3D(values, min, max);

        return values;
	}

    VecVecVecF NoiseGenerator::Worley3D(const glm::uvec3& size, uint32_t cubeSideLength, bool vulkanCompute, float min, float max)
	{
        // Allocate
        VecVecVecF values(size.x);
        for (std::vector<std::vector<float>>& vvf : values)
        {
            vvf.resize(size.y);
            for (std::vector<float>& vf : vvf)
                vf.resize(size.z);
        }

        // Generate random point in cubes
        glm::uvec3 cubePointSize = size / cubeSideLength;
        if (cubePointSize.x == 0)
            cubePointSize.x = 1;
        if (cubePointSize.y == 0)
            cubePointSize.y = 1; 
        if (cubePointSize.z == 0)
            cubePointSize.z = 1;

        std::vector<glm::vec3> cubePoints;
        for (uint32_t i = 0; i < cubePointSize.x; i++)
        {
            for (uint32_t j = 0; j < cubePointSize.y; j++)
            {
                for (uint32_t k = 0; k < cubePointSize.z; k++)
                {
                    glm::vec3 cubePoint = static_cast<glm::vec3>(glm::uvec3(i, j, k));
                    cubePoint += glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f));
                    cubePoint *= static_cast<float>(cubeSideLength);
                    cubePoints.push_back(cubePoint);
                }
            }
        }

        // Sample distance to closest random point
        if (vulkanCompute)
        {
            // Create input buffer
            size_t inBufferSize = sizeof(glm::uvec3) + sizeof(uint32_t) + sizeof(glm::vec3) * cubePoints.size();
            vk::Buffer inBuffer(
                inBufferSize,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                {});

            // Write input data into buffer
            char* data = reinterpret_cast<char*>(malloc(inBufferSize));

            memcpy(data, &size, sizeof(glm::uvec3));

            uint32_t cubePointCount = cubePoints.size();
            memcpy(data + sizeof(glm::uvec3), &cubePointCount, sizeof(uint32_t));

            memcpy(data + sizeof(glm::uvec3) + sizeof(uint32_t), cubePoints.data(), sizeof(glm::vec3) * cubePointCount);

            inBuffer.MapMemory(inBufferSize, data, 0, 0);
            free(data);

            // Create output buffer
            size_t outBufferSize = sizeof(float) * size.x * size.y * size.z;
            vk::Buffer outBuffer(
                outBufferSize,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                {});

            // Create and execute compute task
            vk::ComputeTask computeTask(&inBuffer, &outBuffer, size);
            m_ComputePipeline->Execute(VulkanAPI::GetGraphicsQueue(), VulkanAPI::GetGraphicsQFI(), &computeTask);

            // Reteive results
            std::vector<float> linearResults(outBufferSize / sizeof(float));
            outBuffer.GetData(outBufferSize, linearResults.data(), 0, 0);
            for (uint32_t i = 0; i < size.x; i++)
            {
                for (uint32_t j = 0; j < size.y; j++)
                {
                    for (uint32_t k = 0; k < size.z; k++)
                    {
                        uint32_t index = i + j * size.x + k * size.x * size.y;
                        float value = linearResults[index];
                        values[i][j][k] = value;
                    }
                }
            }

            // Destroy resources
            inBuffer.Destroy();
            outBuffer.Destroy();
            computeTask.Destroy();
        }
        else // !vulkanCompute
        {
            for (uint32_t i = 0; i < size.x; i++)
            {
                for (uint32_t j = 0; j < size.y; j++)
                {
                    for (uint32_t k = 0; k < size.z; k++)
                    {
                        float minDistance = std::numeric_limits<float>::max();
                        for (const glm::vec3& cubePoint : cubePoints) // TODO: optimize
                        {
                            glm::vec3 currentPos = static_cast<glm::vec3>(glm::uvec3(i, j, k));
                            float distance = glm::length(cubePoint - currentPos);
                            if (distance < minDistance)
                                minDistance = distance;
                        }
                        values[i][j][k] = minDistance;
                    }
                }
            }
        }

        // Normalize
        FitInRange3D(values, min, max);

        return values;
	}

    VecVecVecF NoiseGenerator::NoNoise3D(const glm::uvec3& size, float value)
    {
        // Allocate and fill
        VecVecVecF values(size.x);
        for (std::vector<std::vector<float>>& vvf : values)
        {
            vvf.resize(size.y);
            for (std::vector<float>& vf : vvf)
            {
                vf.resize(size.z);
                for (float& f : vf)
                    f = value;
            }
        }

        return values;
    }

    void NoiseGenerator::FitInRange3D(VecVecVecF& values, float targetMin, float targetMax)
    {
        // Calculate current min and max
        float currentMin = std::numeric_limits<float>::max();
        float currentMax = std::numeric_limits<float>::min();
        for (uint32_t x = 0; x < values.size(); x++)
        {
            for (uint32_t y = 0; y < values[x].size(); y++)
            {
                for (uint32_t z = 0; z < values[x][y].size(); z++)
                {
                    float val = values[x][y][z];
                    if (val < currentMin)
                        currentMin = val;
                    if (val > currentMax)
                        currentMax = val;
                }
            }
        }

        // Fit in target range
        for (uint32_t x = 0; x < values.size(); x++)
        {
            for (uint32_t y = 0; y < values[x].size(); y++)
            {
                for (uint32_t z = 0; z < values[x][y].size(); z++)
                {
                    float val = values[x][y][z];
                    float result = (val - currentMin) / (currentMax - currentMin); // In range [0, 1]
                    result = result * (targetMax - targetMin) + targetMin; // In range [targetMin, targetMax]
                    values[x][y][z] = result;
                }
            }
        }
    }
}
