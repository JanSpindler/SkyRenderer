#pragma once

#include "engine/graphics/Atmosphere.hpp"
#include <cassert>
#include <cstdint>
#include <functional>
#include <queue>
#include <vulkan/vulkan_core.h>
#include <memory>
#include <list>

typedef std::vector<VkCommandBuffer>::iterator BufIter;
typedef std::function<VkSemaphore *(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore)> InstantiaterResult;
typedef std::function<InstantiaterResult(uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet &env)> Instantiater;

typedef std::function<void()> CleanupTask;

namespace en {
	class Precomputer {
		public:
			Precomputer(Atmosphere &atmosphere, EnvConditions &env, uint32_t stepsPerScatteringOrder, uint32_t stepsPerFrame, uint32_t blendFrames);
			~Precomputer();
			VkSemaphore *Frame(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore);
			void RenderImgui();
			VkDescriptorSet GetRatioDescriptorSet();
			VkDescriptorSetLayout GetRatioDescriptorSetLayout();

			EnvConditions &GetEffectiveEnv(size_t indx);

		private:
			class PrecomputeTask {
				public:
					PrecomputeTask(
						Instantiater &instantiater,
						uint32_t offset,
						uint32_t count,
						uint32_t sumTarget,
						BufIter &currentBuffer,
						VkDescriptorSet env);

					PrecomputeTask &operator=(PrecomputeTask &other);
					bool Combine(PrecomputeTask &subsequent);
					InstantiaterResult Instantiate();

				private:
					uint32_t m_Offset;
					uint32_t m_Count;
					uint32_t m_SumTarget;
					BufIter &m_CurrentBuffer;
					Instantiater &m_Instantiater;
					VkDescriptorSet m_EnvDescSet;
			};

			EnvConditions &m_Env;

			// two envs for sky, for sumTarget=0/1 respectively.
			EnvConditions m_EffectiveSkyEnv[2];

			uint32_t m_MaxSteps;
			uint32_t m_StepsPerScatteringOrder;
			uint32_t m_StepsPerFrame;

			uint32_t m_BlendFrames;

			VkDescriptorSetLayout m_RatioDescriptorSetLayout;
			VkDescriptorPool m_DescriptorPool;
			VkDescriptorSet m_RatioDescriptorSet;
			vk::Buffer m_SumImageRatioUBO;

			float m_SumImageRatio;

			// stores the target image of the last enqued atmosphere-change.
			size_t m_SumTarget;

			// create and store functions here and pass references to PrecomputeTask.
			Instantiater m_TransmittanceInstantiater;
			Instantiater m_SingleScatteringInstantiater;
			Instantiater m_MultiScatteringInstantiater;
			Instantiater m_GatheringInstantiater;

			std::list<std::vector<InstantiaterResult>> m_FrameTasks;
			std::list<std::vector<InstantiaterResult>>::iterator m_CurrentFrameTask;

			// store in list: erase without invalidating iterators.
			std::list<CleanupTask> m_CleanupTasks;

			bool m_AllTasksRun;

			void _init(size_t sumTarget);
			void Enqueue();

			// all tasks that have to be run to complete precomputation.

			void CreateDescriptor(float initial_value);
			std::deque<PrecomputeTask> CreateTasks(
				uint32_t stepsPerScatteringOrder,
				uint32_t sumTarget,
				BufIter &currentBuffer,
				VkDescriptorSet envDescSet);
			void CreateFrameTasks(
				std::deque<PrecomputeTask> tasks,
				uint32_t stepsPerFrame,
				std::list<CleanupTask>::iterator cleanupIter,
				std::shared_ptr<EnvConditions> env,
				uint32_t sumTarget);
	};
};
