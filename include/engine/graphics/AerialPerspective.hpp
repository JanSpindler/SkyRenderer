#pragma once

#include "engine/graphics/Atmosphere.hpp"
#include "engine/graphics/Precomputer.hpp"
#include "engine/graphics/Sun.hpp"
#include "engine/graphics/vulkan/CommandPool.hpp"
#include "engine/graphics/vulkan/Shader.hpp"
#include "engine/graphics/Camera.hpp"
#include <array>

#define IMAGE_COUNT 2

namespace en {

class AerialPerspective {
	public:
		AerialPerspective(Camera &cam, Precomputer &precomp, Atmosphere &atm, Sun &sun);
		~AerialPerspective();

		void Compute(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore);
		VkDescriptorSet GetSampleDescriptor() const;
		VkDescriptorSetLayout GetSampleDescriptorLayout() const;

	private:
		Camera &m_Cam;
		Precomputer &m_Precomp;
		Atmosphere &m_Atmosphere;
		Sun &m_Sun;

		vk::Shader m_Shader;

		vk::CommandPool m_CommandPool;
		VkCommandBuffer m_ComputeCommandBuffer;
		std::array<VkCommandBuffer, IMAGE_COUNT> m_LayoutCommandBuffer;

		VkDescriptorPool m_DescriptorPool;

		VkFormat m_ComputeImageFormat;

		std::array<VkImage, IMAGE_COUNT> m_APImages;
		std::array<VkDeviceMemory, IMAGE_COUNT> m_APImagesMemory;
		std::array<VkImageView, IMAGE_COUNT> m_APImageViews;

		VkDescriptorSetLayout m_ImageDescriptorLayout;
		VkDescriptorSetLayout m_SampleDescriptorLayout;
		VkDescriptorSet m_APImageDescriptor;

		VkSampler m_LinearSampler;
		VkDescriptorSet m_APImageSampleDescriptor;

		VkPipelineLayout m_APPipelineLayout;
		VkPipeline m_APPipeline;

		void CreateComputeImages(VkDevice device);
		void CreateComputePipeline(VkDevice device);
		void CreateDescriptors(VkDevice device);
		void CreateCommandBuffers();
		void RecordCommandBuffers();
};

};
