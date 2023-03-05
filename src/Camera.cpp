#include <glm/gtx/dual_quaternion.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <engine/graphics/Camera.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/matrix.hpp>
#include <imgui.h>

namespace en
{
	VkDescriptorSetLayout Camera::m_DescriptorSetLayout;
	VkDescriptorPool Camera::m_DescriptorPool;

	void Camera::Init()
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding layoutBinding;
		layoutBinding.binding = 0;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		// calculate viewing direction in aerial_perspective, too.
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		layoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.pNext = nullptr;
		descSetLayoutCreateInfo.flags = 0;
		descSetLayoutCreateInfo.bindingCount = 1;
		descSetLayoutCreateInfo.pBindings = &layoutBinding;

		VkResult result = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create Descriptor Pool
		VkDescriptorPoolSize poolSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = MAX_CAMERA_COUNT;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		result = vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool);
		ASSERT_VULKAN(result);
	}

	void Camera::Shutdown()
	{
		VkDevice device = VulkanAPI::GetDevice();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}

	VkDescriptorSetLayout Camera::GetDescriptorSetLayout()
	{
		return m_DescriptorSetLayout;
	}

	Camera::Camera(
		const glm::vec3& pos,
		const float zenith,
		const glm::vec3& up,
		float width,
		float height,
		float fov,
		float nearPlane,
		float farPlane)
		:
		m_Pos(pos),
		m_Zenith(zenith),
		m_Up(glm::normalize(up)),
		m_Height(height),
		m_Width(width),
		m_Fov(fov),
		m_NearPlane(nearPlane),
		m_FarPlane(farPlane),
		m_ViewDir{glm::vec3(0,0,1)},
		m_UniformBuffer(new vk::Buffer(
			sizeof(CamParams),
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			{}))
	{
		VkDevice device = VulkanAPI::GetDevice();

		// Allocate Descriptor Set
		VkDescriptorSetAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.descriptorPool = m_DescriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &m_DescriptorSetLayout;

		VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &m_DescriptorSet);
		ASSERT_VULKAN(result);

		// Write Descriptor Set
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = m_UniformBuffer->GetVulkanHandle();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(CamParams);

		VkWriteDescriptorSet writeDescSet;
		writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescSet.pNext = nullptr;
		writeDescSet.dstSet = m_DescriptorSet;
		writeDescSet.dstBinding = 0;
		writeDescSet.dstArrayElement = 0;
		writeDescSet.descriptorCount = 1;
		writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescSet.pImageInfo = nullptr;
		writeDescSet.pBufferInfo = &bufferInfo;
		writeDescSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);


		// also sets m_{Height,Width}.
		SetAspectRatio(width, height);
		// Update.
		UpdateUBO();
	}

	void Camera::Destroy()
	{
		m_UniformBuffer->Destroy();
	}

	void Camera::UpdateUBO()
	{
		glm::mat4 projMat = glm::perspective(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
		glm::mat4 viewMat = glm::lookAt(glm::vec3(0,0,0), glm::vec3(m_ViewDir), m_Up);
		// invert before applying translation, better numerical stability.
		glm::mat4 viewMatInv = glm::inverse(viewMat);
			
		viewMat *= glm::translate(-m_Pos);
		viewMatInv = glm::translate(m_Pos) * viewMatInv;

		m_UboData.m_Pos = m_Pos;
		m_UboData.m_projView = projMat * viewMat;
		m_UboData.m_projViewInv = glm::dmat4(viewMatInv) * glm::inverse(glm::dmat4(projMat));
		m_UboData.m_Near = m_NearPlane;
		m_UboData.m_Far = m_FarPlane;
		m_UboData.m_Width = m_Width;
		m_UboData.m_Height = m_Height;
		m_UniformBuffer->MapMemory(sizeof(CamParams), &m_UboData, 0, 0);
	}

	void Camera::RenderImgui()
	{
		ImGui::Begin("Cam");

		ImGui::DragFloat("Height", &m_Pos.y, 1000000, 10, 1000000000, "%g", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloat("zenith", &m_Zenith, 0.001, -FLT_MAX, FLT_MAX);

		ImGui::End();
	}

	void Camera::Move(const glm::vec3& move)
	{
		glm::vec3 frontMove = glm::normalize(m_ViewDir * glm::vec3(1.0f, 0.0f, 1.0f)) * move.z;
		glm::vec3 sideMove = glm::normalize(glm::cross(m_ViewDir, m_Up)) * move.x;
		glm::vec3 upMove(0.0f, move.y, 0.0f);
		m_Pos += frontMove + sideMove + upMove;
	}

	void Camera::RotateViewDir(float phi, float theta)
	{
		glm::vec3 phiAxis = m_Up;
		glm::mat3 phiMat = glm::rotate(glm::identity<glm::mat4>(), phi, phiAxis);

		glm::vec3 thetaAxis = glm::normalize(glm::cross(m_ViewDir, m_Up));
		glm::mat3 thetaMat = glm::rotate(glm::identity<glm::mat4>(), theta, thetaAxis);

		m_ViewDir = glm::normalize(thetaMat * phiMat * m_ViewDir);
	}

	const glm::vec3& Camera::GetPos() const
	{
		return m_Pos;
	}

	void Camera::SetPos(const glm::vec3& pos)
	{
		m_Pos = pos;
	}

	const glm::vec3& Camera::GetViewDir() const
	{
		return m_ViewDir;
	}

	void Camera::SetViewDir(const glm::vec3& viewDir)
	{
		m_ViewDir = glm::normalize(viewDir);
	}

	const glm::vec3& Camera::GetUp() const
	{
		return m_Up;
	}

	void Camera::SetUp(const glm::vec3& up)
	{
		m_Up = glm::normalize(up);
	}

	float Camera::GetAspectRatio() const
	{
		return m_AspectRatio;
	}

	void Camera::SetAspectRatio(float aspectRatio)
	{
		m_AspectRatio = aspectRatio;
	}

	void Camera::SetAspectRatio(uint32_t width, uint32_t height)
	{
		if (height == 0)
			height = 1;
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Width = width;
		m_Height = height;
	}

	float Camera::GetFov() const
	{
		return m_Fov;
	}

	void Camera::SetFov(float fov)
	{
		m_Fov = fov;
	}

	float Camera::GetNearPlane() const
	{
		return m_NearPlane;
	}

	void Camera::SetNearPlane(float nearPlane)
	{
		m_NearPlane= nearPlane;
	}

	float Camera::GetFarPlane() const
	{
		return m_FarPlane;
	}

	void Camera::SetFarPlane(float farPlane)
	{
		m_FarPlane = farPlane;
	}

	VkDescriptorSet Camera::GetDescriptorSet() const
	{
		return m_DescriptorSet;
	}
}
