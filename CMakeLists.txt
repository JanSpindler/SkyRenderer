cmake_minimum_required(VERSION 3.8)

if (DEFINED ENV{VCPKG_CMAKE_INCLUDE})
	include($ENV{VCPKG_CMAKE_INCLUDE})
endif()

set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Project
project(SkyRenderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/bin)

file(GLOB_RECURSE SKY_RENDERER_SOURCE "src/*.cpp")

add_executable(${PROJECT_NAME} ${SKY_RENDERER_SOURCE})
target_include_directories(${PROJECT_NAME} PUBLIC "include")
target_include_directories(${PROJECT_NAME} PUBLIC "shared_include")

# Vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# GLFW
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

# STB
find_path(STB_INCLUDE_DIRS "stb.h")
target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})

# Assimp
find_package(assimp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)

# ImGui
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
