#pragma once

#include <engine/graphics/Common.hpp>
#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/vulkan/CommandPool.hpp>
#include <engine/graphics/Camera.hpp>
#include <engine/objects/CloudData.hpp>
#include <engine/graphics/Sun.hpp>

namespace en
{
	class CloudRenderer
	{
	public:
		CloudRenderer(uint32_t width, uint32_t height, const Camera* camera, const Sun* dirLight, const CloudData* cloudData);

		void Render(VkQueue queue);
		void Destroy();

		void Resize(uint32_t width, uint32_t height);

		VkImageView GetImageView() const;

	private:
		uint32_t m_Width;
		uint32_t m_Height;
		const Camera* m_Camera;
		const Sun* m_Sun;
		const CloudData* m_CloudData;

		VkRenderPass m_RenderPass;

		vk::Shader m_CloudVertShader;
		vk::Shader m_CloudFragShader;
		VkPipelineLayout m_CloudPipelineLayout;
		VkPipeline m_CloudPipeline;

		VkFormat m_ColorFormat;
		VkImage m_ColorImage;
		VkDeviceMemory m_ColorImageMemory;
		VkImageView m_ColorImageView;

		VkFormat m_DepthFormat;
		VkImage m_DepthImage;
		VkDeviceMemory m_DepthImageMemory;
		VkImageView m_DepthImageView;
		
		VkFormat m_TuFormat;
		VkImage m_TuImage;
		VkDeviceMemory m_TuImageMemory;
		VkImageView m_TuImageView;

		vk::Buffer m_TuRandomBuffer;

		VkSampler m_TuSampler;
		VkDescriptorSetLayout m_TuDescriptorSetLayout;
		VkDescriptorPool m_TuDescriptorPool;
		VkDescriptorSet m_TuDescriptorSet;

		VkFramebuffer m_Framebuffer;
		vk::CommandPool m_CommandPool;
		VkCommandBuffer m_CommandBuffer;

		void FindFormats();

		void CreateTuDescriptorResources(VkDevice device);
		void CreateRenderPass(VkDevice device);
		void CreateCloudPipelineLayout(VkDevice device);
		void CreateCloudPipeline(VkDevice device);
		void CreateColorImageResources(VkDevice device);
		void CreateDepthImageResources(VkDevice device);
		void CreateTuImageResources(VkDevice device);
		void CreateFramebuffer(VkDevice device);
		void CreateCommandBuffer();
		void RecordCommandBuffer();
	};
}
