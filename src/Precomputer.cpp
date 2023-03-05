#include "engine/graphics/EnvConditions.hpp"
#include <cstdio>
#include <engine/graphics/Precomputer.hpp>
#include <imgui.h>
#include <iterator>
#include <vulkan/vulkan_core.h>

#include <scattering.h>
#include <gathering.h>

namespace en {

Precomputer::PrecomputeTask::PrecomputeTask(Instantiater &instantiater, uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet env) :
	m_Instantiater{instantiater},
	m_Offset{offset},
	m_Count{count},
	m_SumTarget{sumTarget},
	m_CurrentBuffer{currentBuffer},
	m_EnvDescSet{env} { }

Precomputer::PrecomputeTask &Precomputer::PrecomputeTask::operator=(Precomputer::PrecomputeTask &other) {
	// ref should still refer to same instance.
	return *this;
}

InstantiaterResult Precomputer::PrecomputeTask::Instantiate() {
	return m_Instantiater(m_Offset, m_Count, m_SumTarget, m_CurrentBuffer, m_EnvDescSet);
}

bool Precomputer::PrecomputeTask::Combine(Precomputer::PrecomputeTask &subsequent) {
	if (&subsequent.m_Instantiater == &m_Instantiater &&
		subsequent.m_Offset + subsequent.m_Count == m_Offset) {
			m_Count += subsequent.m_Count;
			return true;
		}

	// couldn't combine this and the subsequent task into one.
	return false;
}

void Precomputer::_init(size_t sumTarget)
{
	// This is an upper limit for the number of buffers/steps, very well possible that fewer are actually needed.
	// Each step gets its own command buffer, so we need stepsPerScatteringOrder for each scattering order.
	// Allocate an additional SCATTERING_ORDERS Buffers for Gathering and one buffer for transmittance.
	m_MaxSteps = SCATTERING_ORDERS*(m_StepsPerScatteringOrder+1)+1;

	// unique would be enough, but it has to be passed to a std::function,
	// which is copyable and therefore cannot deal with an unique_ptr.
	auto commandPool = std::make_shared<vk::CommandPool>(0, VulkanAPI::GetComputeQFI());
	commandPool->AllocateBuffers(m_MaxSteps, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// snapshot current settings so they cannot be changed while precomputing.
	auto envConds = std::make_shared<EnvConditions>(m_Env.GetEnvironment());

	// order of cleanup in ~Precomputer doesn't matter.
	// Will be called either after precomputation is done or in this instances destructor.
	auto cleanupIter = m_CleanupTasks.insert(m_CleanupTasks.begin(),
		[commandPool, envConds]() {
			commandPool->FreeBuffers();
			commandPool->Destroy();
			// envConds goes out of scope here (will not be optimized away as per standard),
			// so its' destructor is called.
		} );

	std::vector<VkCommandBuffer> buffers = commandPool->GetBuffers();
	std::vector<VkCommandBuffer>::iterator currentBuffer = buffers.begin();

	CreateFrameTasks(CreateTasks(m_StepsPerScatteringOrder, sumTarget, currentBuffer, envConds->GetDescriptorSet()), m_StepsPerFrame, cleanupIter, envConds, sumTarget);
	m_CurrentFrameTask = m_FrameTasks.begin();
	m_AllTasksRun = false;
}

InstantiaterResult runBufferFunction(VkCommandBuffer buf) {
	return [buf](VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = waitSemaphore != nullptr ? 1 : 0;
		submitInfo.pWaitSemaphores = waitSemaphore;
		submitInfo.pWaitDstStageMask = &waitFlags;
		submitInfo.signalSemaphoreCount = signalSemaphore != nullptr ? 1 : 0;
		submitInfo.pSignalSemaphores = signalSemaphore;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buf;

		ASSERT_VULKAN(vkQueueSubmit(VulkanAPI::GetComputeQueue(), 1, &submitInfo, VK_NULL_HANDLE));

		return signalSemaphore;
	};
}

Precomputer::Precomputer(
	Atmosphere &atmosphere,
	EnvConditions &env,
	uint32_t stepsPerScatteringOrder,
	uint32_t stepsPerFrame,
	uint32_t blendFrames) :
	m_Env{env},
	// will be overriden in _init, but easier this way.
	m_EffectiveSkyEnv{m_Env, m_Env},
	m_StepsPerScatteringOrder{stepsPerScatteringOrder},
	m_MaxSteps{SCATTERING_ORDERS*(stepsPerScatteringOrder+1)},
	m_StepsPerFrame{stepsPerFrame == UINT32_MAX ? m_MaxSteps : stepsPerFrame},
	m_SumImageRatio{0},
	// write into buffer 0 first, blend from 1 to it after precomputing (maybe in one step?).
	m_SumTarget{0},
	m_BlendFrames{blendFrames},
	m_SumImageRatioUBO(
		sizeof(m_SumImageRatio),
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		{}) {
	
	m_SingleScatteringInstantiater =
		[&atmosphere = atmosphere]
		(uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet env) {

			atmosphere.RecordSingleScatteringCommandBuffer(*currentBuffer, offset, count, sumTarget, env);
			// advance currentBuffer!! Next Instantiation shouldn't use the same buffer.
			return runBufferFunction(*(currentBuffer++));
		};
	m_MultiScatteringInstantiater =
		[&atmosphere = atmosphere]
		(uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet env) {

			atmosphere.RecordMultiScatteringCommandBuffer(*currentBuffer, offset, count, sumTarget, env);
			return runBufferFunction(*(currentBuffer++));
		};
	m_GatheringInstantiater = 
		[&atmosphere = atmosphere]
		(uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet env) {

			atmosphere.RecordGatheringCommandBuffer(*currentBuffer, offset, count, sumTarget, env);
			return runBufferFunction(*(currentBuffer++));
		};
	m_TransmittanceInstantiater = 
		[&atmosphere = atmosphere]
		// offset and count are not really necessary, but it's easier to just ignore them than incorporating
		// the difference properly via a class-hierarchy for the Instantiaters+PrecomputeTasks.
		(uint32_t offset, uint32_t count, uint32_t sumTarget, BufIter &currentBuffer, VkDescriptorSet env) {

			atmosphere.RecordTransmittanceCommandBuffer(*currentBuffer, sumTarget, env);
			return runBufferFunction(*(currentBuffer++));
		};

	_init(m_SumTarget);
	// pass initial value for blend, will use texture 1 (all zero) and then blend to
	// texture 0 after it has been filled by the first precomputation.
	CreateDescriptor(1);
}

Precomputer::~Precomputer() {
	VkDevice device = VulkanAPI::GetDevice();
	vkDestroyDescriptorSetLayout(device, m_RatioDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	for (CleanupTask cleanupTask : m_CleanupTasks)
		cleanupTask();
	m_SumImageRatioUBO.Destroy();
}

std::deque<Precomputer::PrecomputeTask> Precomputer::CreateTasks(
	uint32_t stepsPerScatteringOrder,
	uint32_t sumTarget,
	BufIter &currentBuffer,
	VkDescriptorSet envDescSet) {

	// count for each step for one scattering order.
	std::vector<uint32_t> counts(stepsPerScatteringOrder);

	std::fill_n(counts.begin(), stepsPerScatteringOrder, SCATTERING_RESOLUTION_HEIGHT/stepsPerScatteringOrder);
	// distribute leftover steps.
	for (int i = 0; i != SCATTERING_RESOLUTION_HEIGHT%stepsPerScatteringOrder; ++i)
		counts[i]++;

	// offset for each step for one scattering order.
	std::vector<uint32_t> offsets(stepsPerScatteringOrder);
	offsets[0] = 0;
	for (int i = 1; i != stepsPerScatteringOrder; ++i)
		offsets[i] = offsets[i-1] + counts[i-1];


	std::deque<PrecomputeTask> tasks;

	// don't need offset or count, transmittance is very fast so there's no need to split it up.
	tasks.emplace_back(m_TransmittanceInstantiater, 0,0, sumTarget, currentBuffer, envDescSet);

	for (int i = 0; i != stepsPerScatteringOrder; ++i)
		tasks.emplace_back(m_SingleScatteringInstantiater, offsets[i], counts[i], sumTarget, currentBuffer, envDescSet);
	// gathering doesn't need to be split up either.
	tasks.emplace_back(m_GatheringInstantiater, 0, GATHERING_RESOLUTION_HEIGHT, sumTarget, currentBuffer, envDescSet);

	for (int i = 1; i != SCATTERING_ORDERS; ++i) {
		for (int j = 0; j != stepsPerScatteringOrder; ++j)
			tasks.emplace_back(m_MultiScatteringInstantiater, offsets[j], counts[j], sumTarget, currentBuffer, envDescSet);
		tasks.emplace_back(m_GatheringInstantiater, 0, GATHERING_RESOLUTION_HEIGHT, sumTarget, currentBuffer, envDescSet);
	}

	return tasks;
}

void Precomputer::CreateFrameTasks(
	std::deque<Precomputer::PrecomputeTask> tasks,
	uint32_t stepsPerFrame,
	std::list<CleanupTask>::iterator cleanupIter,
	std::shared_ptr<EnvConditions> env,
	uint32_t sumTarget) {

	// As long as there are tasks, remove stepsPerFrame many and append them to the tasks for one frame.
	// TODO: optimize by not popping, but simply increasing a iterator until it reaches the end.
	while (!tasks.empty()) {
		std::vector<InstantiaterResult> tasksForFrame;
		// pop first into currentTask.
		PrecomputeTask currentTask = tasks.front();
		tasks.pop_front();
		for (int i = 1; i != stepsPerFrame && !tasks.empty(); ++i) {
			if (!currentTask.Combine(tasks.front())) {
				tasksForFrame.push_back(currentTask.Instantiate());
				currentTask = tasks.front();
			}
			// in both cases tasks is included in tasksForFrame:
			// either it was combined with currentTask or it is now currentTask
			tasks.pop_front();
		}
		tasksForFrame.push_back(currentTask.Instantiate());
		m_FrameTasks.push_back(tasksForFrame);
	}

	m_FrameTasks.push_back({
		// update environment used by sky before blending starts.
		[env = env->GetEnvironment(), envConds = &m_EffectiveSkyEnv[sumTarget]]
		(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
			envConds->SetEnvironment(env);

			return nullptr;
		},

		// perform cleanup for the tasks just performed.
		// (erase to prevent re-releasing resources).
		[cleanupIter, cleanupTasks = &m_CleanupTasks]
		(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
			(*cleanupIter)();
			cleanupTasks->erase(cleanupIter);

			return nullptr;
		}
	});

	// append functions for blending.
	int start, end, diff;
	if (m_SumTarget == 0) {
		// is 1, blend from 9.something to 0.
		start = m_BlendFrames-1;
		// include 0.
		end = -1;
		// count down.
		diff = -1;
	} else {
		// is 0, blend from 0.something to 1.
		start = 1;
		end = m_BlendFrames+1;
		diff = 1;
	}
	for (int i = start; i != end; i = i + diff) {
		float ratio = i/float(m_BlendFrames);
		m_FrameTasks.push_back({
			[ratio, &ubo = m_SumImageRatioUBO]
			(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore){
				ubo.MapMemory(sizeof(ratio), &ratio, 0, 0);

				return nullptr;
			}
		});
	}
}

VkSemaphore *Precomputer::Frame(VkSemaphore *waitSemaphore, VkPipelineStageFlags waitFlags, VkSemaphore *signalSemaphore) {
	if (m_FrameTasks.begin() == m_FrameTasks.end())
		return nullptr;
	VkSemaphore *nextSignalSemaphore;
	// we have at least one task group, run and remove it.
	for (InstantiaterResult task : m_FrameTasks.front()) {
		nextSignalSemaphore = task(waitSemaphore, waitFlags, waitSemaphore);
	}
	m_FrameTasks.pop_front();

	return nextSignalSemaphore;
}

void Precomputer::Enqueue() {
	// 1->0, 0->1.
	m_SumTarget ^= 1;
	_init(m_SumTarget);
} 

void Precomputer::RenderImgui() {
	ImGui::Begin("Precompute");
	ImGui::SliderInt("StepsPerScatteringOrder", (int *)&m_StepsPerScatteringOrder, 1, SCATTERING_RESOLUTION_HEIGHT);
	ImGui::SliderInt("StepsPerFrame", (int *)&m_StepsPerFrame, 1, SCATTERING_ORDERS*(m_StepsPerScatteringOrder+1));
	ImGui::DragInt("BlendFrames", (int *) &m_BlendFrames);

	if (ImGui::Button("Enqueue"))
		Enqueue();
	ImGui::End();
}

void Precomputer::CreateDescriptor(float initial_value) {
		VkDevice device = VulkanAPI::GetDevice();

		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding layoutBinding;
		layoutBinding.binding = 0;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		// access from fragment (atmosphere.frag) and compute (aerial perspective).
		layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		layoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.pNext = nullptr;
		descSetLayoutCreateInfo.flags = 0;
		descSetLayoutCreateInfo.bindingCount = 1;
		descSetLayoutCreateInfo.pBindings = &layoutBinding;

		VkResult result = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &m_RatioDescriptorSetLayout);
		ASSERT_VULKAN(result);

		// Create Descriptor Pool
		VkDescriptorPoolSize poolSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo descPoolCreateInfo;
		descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descPoolCreateInfo.pNext = nullptr;
		descPoolCreateInfo.flags = 0;
		descPoolCreateInfo.maxSets = 1;
		descPoolCreateInfo.poolSizeCount = 1;
		descPoolCreateInfo.pPoolSizes = &poolSize;

		result = vkCreateDescriptorPool(device, &descPoolCreateInfo, nullptr, &m_DescriptorPool);
		ASSERT_VULKAN(result);

		// Allocate Descriptor Set
		VkDescriptorSetAllocateInfo allocateInfo;
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		allocateInfo.descriptorPool = m_DescriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &m_RatioDescriptorSetLayout;

		ASSERT_VULKAN(vkAllocateDescriptorSets(device, &allocateInfo, &m_RatioDescriptorSet));

		// Write Descriptor Set
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = m_SumImageRatioUBO.GetVulkanHandle();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(m_SumImageRatio);

		VkWriteDescriptorSet writeDescSet;
		writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescSet.pNext = nullptr;
		writeDescSet.dstSet = m_RatioDescriptorSet;
		writeDescSet.dstBinding = 0;
		writeDescSet.dstArrayElement = 0;
		writeDescSet.descriptorCount = 1;
		writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescSet.pImageInfo = nullptr;
		writeDescSet.pBufferInfo = &bufferInfo;
		writeDescSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

		m_SumImageRatioUBO.MapMemory(sizeof(initial_value), &initial_value, 0, 0);
}

VkDescriptorSet Precomputer::GetRatioDescriptorSet() { return m_RatioDescriptorSet; }
VkDescriptorSetLayout Precomputer::GetRatioDescriptorSetLayout() { return m_RatioDescriptorSetLayout; }
EnvConditions &Precomputer::GetEffectiveEnv(size_t indx) { return m_EffectiveSkyEnv[indx]; }

};
