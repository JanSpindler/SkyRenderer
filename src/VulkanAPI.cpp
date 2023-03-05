#include <engine/graphics/VulkanAPI.hpp>
#include <vector>
#include <engine/util/Log.hpp>
#include <engine/graphics/Window.hpp>
#include <engine/graphics/Camera.hpp>
#include <engine/graphics/vulkan/Texture2D.hpp>
#include <engine/objects/Material.hpp>
#include <engine/objects/Model.hpp>
#include <engine/objects/CloudData.hpp>
#include <engine/graphics/vulkan/ComputePipeline.hpp>
#include <engine/util/NoiseGenerator.hpp>
#include <engine/graphics/Sun.hpp>

namespace en
{
	VkInstance VulkanAPI::m_Instance;

	VkSurfaceKHR VulkanAPI::m_Surface;
	VkSurfaceCapabilitiesKHR VulkanAPI::m_SurfaceCapabilities;
	VkSurfaceFormatKHR VulkanAPI::m_SurfaceFormat;
	VkPresentModeKHR VulkanAPI::m_PresentMode;

	PhysicalDeviceInfo VulkanAPI::m_PhysicalDeviceInfo;
	uint32_t VulkanAPI::m_GraphicsQFI;
	uint32_t VulkanAPI::m_PresentQFI;

	VkDevice VulkanAPI::m_Device;
	VkQueue VulkanAPI::m_GraphicsQueue;
	VkQueue VulkanAPI::m_PresentQueue;

	void VulkanAPI::Init(const std::string& appName)
	{
		Log::Info("Initializing VulkanAPI");

		CreateInstance(appName);
		m_Surface = Window::CreateVulkanSurface(m_Instance);
		PickPhysicalDevice();
		CreateDevice();

		Camera::Init();
		vk::Texture2D::Init();
		Material::Init();
		ModelInstance::Init();
		CloudData::Init();
		vk::ComputePipeline::Init();
		NoiseGenerator::Init();
		Sun::Init();
	}

	void VulkanAPI::Shutdown()
	{
		Log::Info("Shutting down VulkanAPI");

		Sun::Shutdown();
		NoiseGenerator::Shutdown();
		vk::ComputePipeline::Shutdown();
		CloudData::Shutdown();
		ModelInstance::Shutdown();
		Material::Shutdown();
		vk::Texture2D::Shutdown();
		Camera::Shutdown();

		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyInstance(m_Instance, nullptr);
	}

	uint32_t VulkanAPI::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		const VkPhysicalDeviceMemoryProperties& memoryProperties = m_PhysicalDeviceInfo.memoryProperties;
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
	}

	bool VulkanAPI::IsFormatSupported(VkFormat format, VkImageTiling imageTiling, VkFormatFeatureFlags featureFlags)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(m_PhysicalDeviceInfo.vulkanHandle, format, &formatProperties);

		return (
			imageTiling == VK_IMAGE_TILING_LINEAR && (formatProperties.linearTilingFeatures & featureFlags) == featureFlags ||
			imageTiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties.optimalTilingFeatures & featureFlags) == featureFlags);
	}

	VkFormat VulkanAPI::FindSupportedFormat(
		const std::vector<VkFormat>& formats,
		VkImageTiling imageTiling,
		VkFormatFeatureFlags featureFlags)
	{
		for (size_t i = 0; i < formats.size(); i++)
		{
			VkFormat format = formats[i];
			if (IsFormatSupported(format, imageTiling, featureFlags))
				return format;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkInstance VulkanAPI::GetInstance()
	{
		return m_Instance;
	}

	VkSurfaceKHR VulkanAPI::GetSurface()
	{
		return m_Surface;
	}

	uint32_t VulkanAPI::GetSwapchainImageCount()
	{
		return std::min(m_SurfaceCapabilities.minImageCount + 1, m_SurfaceCapabilities.maxImageCount);
	}

	VkSurfaceFormatKHR VulkanAPI::GetSurfaceFormat()
	{
		return m_SurfaceFormat;
	}

	VkPresentModeKHR VulkanAPI::GetPresentMode()
	{
		return m_PresentMode;
	}

	VkPhysicalDevice VulkanAPI::GetPhysicalDevice()
	{
		return m_PhysicalDeviceInfo.vulkanHandle;
	}

	uint32_t VulkanAPI::GetGraphicsQFI()
	{
		return m_GraphicsQFI;
	}

	uint32_t VulkanAPI::GetPresentQFI()
	{
		return m_PresentQFI;
	}

	VkDevice VulkanAPI::GetDevice()
	{
		return m_Device;
	}

	VkQueue VulkanAPI::GetGraphicsQueue()
	{
		return m_GraphicsQueue;
	}

	VkQueue VulkanAPI::GetPresentQueue()
	{
		return m_PresentQueue;
	}

	void VulkanAPI::CreateInstance(const std::string& appName)
	{
		// List supported layers
		uint32_t supportedLayerCount;
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());

		Log::Info("Supported Instance Layers:");
		for (const VkLayerProperties& layer : supportedLayers)
			Log::Info("\t-" + std::string(layer.layerName) + " | " + std::string(layer.description));

		// Select wanted layers
		std::vector<const char*> layers = {
			"VK_LAYER_KHRONOS_validation"
		};

		// List supported extensions
		uint32_t supportedExtensionCount;
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());

		Log::Info("Supported Instance Extensions:");
		for (const VkExtensionProperties& extension : supportedExtensions)
			Log::Info("\t-" + std::string(extension.extensionName));

		// Select wanted extensions
		std::vector<const char*> extensions = Window::GetVulkanExtensions();

		// Create
		VkApplicationInfo appInfo;
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = appName.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = layers.size();
		createInfo.ppEnabledLayerNames = layers.data();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
		ASSERT_VULKAN(result);
	}

	void VulkanAPI::PickPhysicalDevice()
	{
		// Enumerate physical devices
		uint32_t physicalDeviceCount;
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, nullptr);
		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, physicalDevices.data());

		// Retreive physical device infos
		std::vector<PhysicalDeviceInfo> physicalDeviceInfos(physicalDeviceCount);
		for (uint32_t i = 0; i < physicalDeviceCount; i++)
		{
			VkPhysicalDevice physicalDevice = physicalDevices[i];
			PhysicalDeviceInfo physicalDeviceInfo;
			
			physicalDeviceInfo.vulkanHandle = physicalDevice;
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceInfo.properties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceInfo.features);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceInfo.memoryProperties);

			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			physicalDeviceInfo.queueFamilies.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, physicalDeviceInfo.queueFamilies.data());

			physicalDeviceInfos[i] = physicalDeviceInfo;
		}

		// List physical devices
		Log::Info("Physical devices detected:");
		for (const PhysicalDeviceInfo& physicalDeviceInfo : physicalDeviceInfos)
		{
			Log::Info("\t-" + std::string(physicalDeviceInfo.properties.deviceName));

			const std::vector<VkQueueFamilyProperties>& queueFamilies = physicalDeviceInfo.queueFamilies;
			for (uint32_t i = 0; i < queueFamilies.size(); i++)
			{
				const VkQueueFamilyProperties& queueFamily = queueFamilies[i];
				Log::Info(
					"\t  -Queue Family " + std::to_string(i) +
					": Count(" + std::to_string(queueFamily.queueCount) +
					") | Graphics(" + std::to_string((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) + ")" +
					") | Compute(" + std::to_string((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) + ")" +
					") | Transfer(" + std::to_string((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) + ")" +
					") | SparseBinding(" + std::to_string((queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) + ")" +
					") | Protected(" + std::to_string((queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) != 0) + ")");
			}
		}

		// Pick best physical device
		m_PhysicalDeviceInfo.vulkanHandle = VK_NULL_HANDLE;
		for (const PhysicalDeviceInfo& physicalDeviceInfo : physicalDeviceInfos)
		{
			VkPhysicalDevice physicalDevice = physicalDeviceInfo.vulkanHandle;
			const VkPhysicalDeviceProperties& properties = physicalDeviceInfo.properties;
			const VkPhysicalDeviceFeatures& features = physicalDeviceInfo.features;
			const std::vector<VkQueueFamilyProperties>& queueFamilies = physicalDeviceInfo.queueFamilies;

			// GPU Type
			bool isDiscreteGPU = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

			// Geometry Shader Support
			bool hasGeometryShader = features.geometryShader == VK_TRUE;

			// Graphics QFI
			uint32_t graphicsQFI = UINT32_MAX;
			for (size_t i = 0; i < queueFamilies.size(); i++)
			{
				if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					graphicsQFI = i;
					break;
				}
			}

			// Present Support / QFI
			uint32_t presentQFI = UINT32_MAX;
			VkBool32 presentSupport = VK_FALSE;
			for (size_t i = 0; i < queueFamilies.size(); i++)
			{
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &presentSupport);
				if (presentSupport == VK_TRUE)
				{
					presentQFI = i;
					break;
				}
			}

			// Surface support
			VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &surfaceCapabilities);

			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);
			std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, surfaceFormats.data());

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);
			std::vector<VkPresentModeKHR> presentModes(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data());

			bool formatAvailable = false;
			VkSurfaceFormatKHR bestFormat;
			for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats)
			{
				if (surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
				{
					formatAvailable = true;
					bestFormat = surfaceFormat;
					break;
				}
			}

			VkPresentModeKHR bestPresentMode = VK_PRESENT_MODE_FIFO_KHR;
			for (const VkPresentModeKHR& presentMode : presentModes)
			{
				if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					bestPresentMode = presentMode;
					break;
				}
			}

			// Final check
			if (isDiscreteGPU &&
				hasGeometryShader &&
				graphicsQFI != UINT32_MAX &&
				presentQFI != UINT32_MAX &&
				formatAvailable)
			{
				Log::Info(
					"Picking " + std::string(properties.deviceName) +
					": Graphics QFI(" + std::to_string(graphicsQFI) + ")");

				m_PhysicalDeviceInfo = physicalDeviceInfo;
				m_GraphicsQFI = graphicsQFI;
				m_PresentQFI = presentQFI;

				m_SurfaceCapabilities = surfaceCapabilities;
				m_SurfaceFormat = bestFormat;
				m_PresentMode = bestPresentMode;

				break;
			}
		}

		if (m_PhysicalDeviceInfo.vulkanHandle == VK_NULL_HANDLE)
			Log::Error("Failed to pick physical device", true);
	}

	void VulkanAPI::CreateDevice()
	{
		VkPhysicalDevice physicalDevice = m_PhysicalDeviceInfo.vulkanHandle;

		// List supported layers
		uint32_t supportedLayerCount;
		vkEnumerateDeviceLayerProperties(physicalDevice, &supportedLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
		vkEnumerateDeviceLayerProperties(physicalDevice, &supportedLayerCount, supportedLayers.data());

		Log::Info("Supported device layers:");
		for (const VkLayerProperties& layer : supportedLayers)
			Log::Info("\t-" + std::string(layer.layerName) + " | " + std::string(layer.description));

		// Select wanted layers
		std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

		// List supported extensions
		uint32_t supportedExtensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.data());

		Log::Info("Supported device extensions:");
		for (const VkExtensionProperties& extension : supportedExtensions)
			Log::Info("\t-" + std::string(extension.extensionName));

		// Select wanted extensions
		std::vector<const char*> extensions = { 
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME };

		// QueueFamily create infos
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		if (m_GraphicsQFI == m_PresentQFI)
		{
			float priorities[] = { 1.0f, 1.0f };

			VkDeviceQueueCreateInfo queueCreateInfo;
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.pNext = nullptr;
			queueCreateInfo.flags = 0;
			queueCreateInfo.queueFamilyIndex = m_GraphicsQFI;
			queueCreateInfo.queueCount = 2;
			queueCreateInfo.pQueuePriorities = priorities;

			queueCreateInfos = { queueCreateInfo };
		}
		else
		{
			float priorities[] = { 1.0f };

			VkDeviceQueueCreateInfo graphicsQueueCreateInfo;
			graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			graphicsQueueCreateInfo.pNext = nullptr;
			graphicsQueueCreateInfo.flags = 0;
			graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQFI;
			graphicsQueueCreateInfo.queueCount = 1;
			graphicsQueueCreateInfo.pQueuePriorities = priorities;

			VkDeviceQueueCreateInfo presentQueueCreateInfo;
			presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			presentQueueCreateInfo.pNext = nullptr;
			presentQueueCreateInfo.flags = 0;
			presentQueueCreateInfo.queueFamilyIndex = m_PresentQFI;
			presentQueueCreateInfo.queueCount = 1;
			presentQueueCreateInfo.pQueuePriorities = priorities;
		
			queueCreateInfos = { graphicsQueueCreateInfo, presentQueueCreateInfo };
		}

		// Create
		VkDeviceCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.queueCreateInfoCount = queueCreateInfos.size();
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.enabledLayerCount = layers.size();
		createInfo.ppEnabledLayerNames = layers.data();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();
		createInfo.pEnabledFeatures = nullptr;

		VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_Device);
		ASSERT_VULKAN(result);

		// Retreive device queues
		if (m_GraphicsQFI == m_PresentQFI)
		{
			vkGetDeviceQueue(m_Device, m_GraphicsQFI, 0, &m_GraphicsQueue);
			vkGetDeviceQueue(m_Device, m_GraphicsQFI, 1, &m_PresentQueue);
		}
		else
		{
			vkGetDeviceQueue(m_Device, m_GraphicsQFI, 0, &m_GraphicsQueue);
			vkGetDeviceQueue(m_Device, m_PresentQFI, 0, &m_PresentQueue);
		}
	}
}
