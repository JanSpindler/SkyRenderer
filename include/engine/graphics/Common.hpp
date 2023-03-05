#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <engine/util/Log.hpp>

#define ASSERT_VULKAN(result) if (result != VK_SUCCESS) { en::Log::Error("ASSERT_VULKAN triggered", true); }
