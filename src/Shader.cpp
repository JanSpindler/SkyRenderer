#include <engine/graphics/vulkan/Shader.hpp>
#include <engine/graphics/VulkanAPI.hpp>
#include <engine/util/Log.hpp>
#include <engine/util/ReadFileBinary.hpp>

const std::string compilerPath = "glslc";
const std::string shaderDirPath = "data/shader/";

namespace en::vk
{
	Shader::Shader(const std::vector<char>& code)
	{
		Create(code);
	}

	Shader::Shader(const std::string& fileName, bool compiled)
	{
		std::string fullFilePath = shaderDirPath + fileName;

		std::string outputFileName = fullFilePath;

		if (!compiled)
		{
			outputFileName += ".spv";
			std::string command = compilerPath + " " +
			                      fullFilePath +
			                      " -o " + outputFileName +
			                      " -I shared_include" +
			                      // shaderDirPath includes '/'.
			                      " -I " + shaderDirPath + "include";
			Log::Info("Shader Compile Command: " + command);

			// Compile
			std::system(command.c_str());
		}

		Create(ReadFileBinary(outputFileName));
	}

	void Shader::Destroy()
	{
		vkDestroyShaderModule(VulkanAPI::GetDevice(), m_VulkanModule, nullptr);
	}

	VkShaderModule Shader::GetVulkanModule() const
	{
		return m_VulkanModule;
	}

	void Shader::Create(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkResult result = vkCreateShaderModule(VulkanAPI::GetDevice(), &createInfo, nullptr, &m_VulkanModule);
		ASSERT_VULKAN(result);
	}
}
