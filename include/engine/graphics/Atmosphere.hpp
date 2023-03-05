#pragma once

#include "engine/graphics/vulkan/CommandPool.hpp"
#include "engine/graphics/vulkan/Shader.hpp"
#include "engine/graphics/EnvConditions.hpp"
#include <array>

namespace en {
	class Atmosphere {
		public:
			Atmosphere(VkDescriptorSetLayout envLayout);
			~Atmosphere();

			void Precompute() const;

			VkDescriptorSetLayout GetScatteringSampleDescriptorLayout() const;
			VkDescriptorSet GetScatteringSampleDescriptorSet(size_t sum_target) const;

			VkDescriptorSetLayout GetTransmittanceSampleDescriptorLayout() const;
			VkDescriptorSet GetTransmittanceSampleDescriptorSet(size_t sum_target) const;

			VkDescriptorSetLayout GetGatheringSampleDescriptorLayout() const;
			VkDescriptorSet GetGatheringSampleDescriptorSet(size_t sum_target) const;

			EnvConditions &GetEnv();

			void RecordTransmittanceCommandBuffer(VkCommandBuffer buf, size_t sum_target, VkDescriptorSet env);
			void RecordSingleScatteringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env);
			void RecordMultiScatteringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env);
			void RecordGatheringCommandBuffer(VkCommandBuffer buf, uint32_t offset, uint32_t count, size_t sum_target, VkDescriptorSet env);

		private:
			VkDescriptorSetLayout m_EnvLayout;

			vk::Shader m_SingleShader;
			vk::Shader m_MultiShader;
			vk::Shader m_GatheringShader;
			vk::Shader m_TransmittanceShader;
			
			vk::CommandPool m_ComputeCommandPool;
			VkCommandBuffer m_SingleScatteringBuffer;
			VkCommandBuffer m_MultiScatteringBuffer;
			VkCommandBuffer m_GatheringBuffer;

			VkDescriptorPool m_DescriptorPool;

			VkFormat m_ComputeImageFormat;

			VkImage m_ScatteringImage;
			VkDeviceMemory m_ScatteringImageMemory;
			VkImageView m_ScatteringImageView;

			std::array<VkImage, 2> m_ScatteringSumImage;
			std::array<VkDeviceMemory, 2> m_ScatteringSumImageMemory;
			std::array<VkImageView, 2> m_ScatteringSumImageView;

			VkImage m_GatheringImage;
			VkDeviceMemory m_GatheringImageMemory;
			VkImageView m_GatheringImageView;

			std::array<VkImage, 2> m_GatheringSumImage;
			std::array<VkDeviceMemory, 2> m_GatheringSumImageMemory;
			std::array<VkImageView, 2> m_GatheringSumImageView;

			std::array<VkImage, 2> m_TransmittanceImage;
			std::array<VkDeviceMemory, 2> m_TransmittanceImageMemory;
			std::array<VkImageView, 2> m_TransmittanceImageView;

			vk::CommandPool m_LayoutCommandPool;
			std::vector<VkCommandBuffer> m_LayoutCommandBuffers;


			VkDescriptorSetLayout m_ImageDescriptorLayout;
			VkDescriptorSet m_ScatteringImageDescriptor;
			std::array<VkDescriptorSet, 2> m_ScatteringSumImageDescriptor;
			VkDescriptorSet m_GatheringImageDescriptor;
			std::array<VkDescriptorSet, 2> m_GatheringSumImageDescriptor;
			std::array<VkDescriptorSet, 2> m_TransmittanceImageDescriptor;

			VkDescriptorSetLayout m_SampleDescriptorLayout;
			VkSampler m_LinearSampler;
			VkDescriptorSet m_ScatteringSampleDescriptor;
			std::array<VkDescriptorSet, 2> m_ScatteringSumSampleDescriptor;
			VkDescriptorSet m_GatheringSampleDescriptor;
			std::array<VkDescriptorSet, 2> m_GatheringSumSampleDescriptor;
			std::array<VkDescriptorSet, 2> m_TransmittanceSampleDescriptor;

			VkPipelineLayout m_SSPipelineLayout;
			VkPipeline m_SSPipeline;

			VkPipelineLayout m_MSPipelineLayout;
			VkPipeline m_MSPipeline;

			VkPipelineLayout m_GPipelineLayout;
			VkPipeline m_GPipeline;

			VkPipelineLayout m_TPipelineLayout;
			VkPipeline m_TPipeline;

			void CreateComputeImages(VkDevice device);
			void CreateComputePipeline(VkDevice device);
			void CreateDescriptors(VkDevice device);
			void CreateCommandBuffers();
	};
}
