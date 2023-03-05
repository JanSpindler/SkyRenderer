#pragma once

#include "engine/graphics/Atmosphere.hpp"
#include "engine/graphics/Precomputer.hpp"
#include "engine/graphics/Sun.hpp"
#include "engine/graphics/vulkan/CommandPool.hpp"
#include "engine/graphics/vulkan/Shader.hpp"
#include "engine/graphics/Camera.hpp"
#include <array>

#define CUBE_FACES 6

namespace en {

class GroundLighting {
	public:
		GroundLighting(Precomputer &precomp, Atmosphere &atm, Sun &sun);
		~GroundLighting();

		void Compute();
		VkDescriptorSet GetSampleDescriptorSet() const;
		VkDescriptorSetLayout GetSampleDescriptorLayout() const;

	private:
		Precomputer &m_Precomp;
		Atmosphere &m_Atmosphere;
		Sun &m_Sun;

		vk::Shader m_Shader;

		vk::CommandPool m_CommandPool;
		VkCommandBuffer m_ComputeCommandBuffer;
		VkCommandBuffer m_LayoutCommandBuffer;

		VkDescriptorPool m_DescriptorPool;

		VkFormat m_ComputeImageFormat;

		VkImage m_GLImage;
		VkDeviceMemory m_GLImagesMemory;
		VkImageView m_CubeImageView;
		VkImageView m_ImageView;

		VkDescriptorSetLayout m_ImageDescriptorLayout;
		VkDescriptorSetLayout m_SampleDescriptorLayout;
		VkDescriptorSet m_CubemapImageDescriptor;

		VkSampler m_LinearSampler;
		VkDescriptorSet m_CubemapSampleDescriptor;

		VkPipelineLayout m_GLPipelineLayout;
		VkPipeline m_GLPipeline;

		void CreateComputeImages(VkDevice device);
		void CreateComputePipeline(VkDevice device);
		void CreateDescriptors(VkDevice device);
		void CreateCommandBuffers();
		void RecordCommandBuffers();
		void GenerateVectors();
};

};
