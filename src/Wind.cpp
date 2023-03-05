#include <engine/objects/Wind.hpp>

namespace en
{
	VkDescriptorSetLayout Wind::m_DescriptorSetLayout;
	VkDescriptorPool Wind::m_DescriptorPool;

	void Wind::Init()
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Create descriptor set layout;
		VkDescriptorSetLayoutBinding uniformBufferBinding;
		uniformBufferBinding.binding = 0;
		uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBufferBinding.descriptorCount = 1;
		uniformBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		uniformBufferBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutCI;
		layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCI.pNext = nullptr;
		layoutCI.flags = 0;
		layoutCI.bindingCount = 1;
		layoutCI.pBindings = &uniformBufferBinding;

		VkResult result = vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_DescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create descriptor pool
		VkDescriptorPoolSize uniformBufferPoolSize;
		uniformBufferPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBufferPoolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolCI;
		poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCI.pNext = nullptr;
		poolCI.flags = 0;
		poolCI.maxSets = MAX_COUNT;
		poolCI.poolSizeCount = 1;
		poolCI.pPoolSizes = &uniformBufferPoolSize;

		result = vkCreateDescriptorPool(device, &poolCI, nullptr, &m_DescriptorPool);
		ASSERT_VULKAN(result);
	}

	void Wind::Shutdown()
	{
		VkDevice device = VulkanAPI::GetDevice();
		
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}
	
	VkDescriptorSetLayout Wind::GetDescriptorSetLayout()
	{
		return m_DescriptorSetLayout;
	}

	Wind::Wind(float angle, float strength) :
		m_Angle(angle),
		m_Strength(strength),
		m_UniformData({ glm::vec2(0.0f) }),
		m_UniformBuffer(
			sizeof(WindUniformData), 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			{})
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Allocate descriptor set
		VkDescriptorSetAllocateInfo descSetAI;
		descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetAI.pNext = nullptr;
		descSetAI.descriptorPool = m_DescriptorPool;
		descSetAI.descriptorSetCount = 1;
		descSetAI.pSetLayouts = &m_DescriptorSetLayout;

		VkResult result = vkAllocateDescriptorSets(device, &descSetAI, &m_DescriptorSet);
		ASSERT_VULKAN(result);

		// Write descriptor set
		VkDescriptorBufferInfo uniformBufferInfo;
		uniformBufferInfo.buffer = m_UniformBuffer.GetVulkanHandle();
		uniformBufferInfo.offset = 0;
		uniformBufferInfo.range = sizeof(WindUniformData);

		VkWriteDescriptorSet uniformBufferWrite;
		uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniformBufferWrite.pNext = nullptr;
		uniformBufferWrite.dstSet = m_DescriptorSet;
		uniformBufferWrite.dstBinding = 0;
		uniformBufferWrite.dstArrayElement = 0;
		uniformBufferWrite.descriptorCount = 1;
		uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBufferWrite.pImageInfo = nullptr;
		uniformBufferWrite.pBufferInfo = &uniformBufferInfo;
		uniformBufferWrite.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &uniformBufferWrite, 0, nullptr);

		// Update buffer
		Update(0.0f);
	}

	void Wind::Destroy()
	{
		m_UniformBuffer.Destroy();
	}

	void Wind::Update(float deltaTime)
	{
		m_UniformData.offset += glm::vec2(glm::sin(m_Angle), glm::cos(m_Angle)) * m_Strength * deltaTime;
		m_UniformData.offset.x = fmod(m_UniformData.offset.x, 8192.0f); // fmod create a visible jump while sampling -> dont use often
		m_UniformData.offset.y = fmod(m_UniformData.offset.y, 8192.0f);

		m_UniformBuffer.MapMemory(sizeof(WindUniformData), &m_UniformData, 0, 0);
	}

	VkDescriptorSet Wind::GetDescriptorSet() const
	{
		return m_DescriptorSet;
	}
}
