#pragma once

#include <engine/graphics/Common.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <engine/graphics/vulkan/Buffer.hpp>

// properly aligned.
struct CamParams {
	glm::dmat4 m_projViewInv;
	glm::mat4 m_projView;
	glm::vec3 m_Pos;
	float m_Near;
	float m_Far;
	float m_Width;
	float m_Height;
};

namespace en
{
	const uint32_t MAX_CAMERA_COUNT = 16;

	class Camera
	{
	public:
		static void Init();
		static void Shutdown();

		static VkDescriptorSetLayout GetDescriptorSetLayout();

		Camera(const glm::vec3& pos, const float zenith, const glm::vec3& up, float width, float height, float fov, float nearPlane, float farPlane);

		void RenderImgui();

		void Destroy();
		void UpdateUBO();

		const glm::vec3& GetPos() const;
		void SetPos(const glm::vec3& pos);

		const glm::vec3& GetViewDir() const;
		void SetViewDir(const glm::vec3& viewDir);

		const glm::vec3& GetUp() const;
		void SetUp(const glm::vec3& up);

		float GetAspectRatio() const;
		void SetAspectRatio(float aspectRatio);
		void SetAspectRatio(uint32_t width, uint32_t height);

		float GetFov() const;
		void SetFov(float fov);

		float GetNearPlane() const;
		void SetNearPlane(float nearPlane);

		float GetFarPlane() const;
		void SetFarPlane(float farPlane);

		VkDescriptorSet GetDescriptorSet() const;

	private:
		static VkDescriptorSetLayout m_DescriptorSetLayout;
		static VkDescriptorPool m_DescriptorPool;

		CamParams m_UboData;

		float m_Zenith;

		glm::vec3 m_Pos;
		glm::vec3 m_ViewDir;
		glm::vec3 m_Up;
		float m_AspectRatio;
		float m_Fov;
		float m_NearPlane;
		float m_FarPlane;
		float m_Width;
		float m_Height;

		VkDescriptorSet m_DescriptorSet;
		vk::Buffer* m_UniformBuffer;
	};
}
