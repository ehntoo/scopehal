#ifndef ScopehalVulkanUtils_h
#define ScopehalVulkanUtils_h

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

//Shader args for frequently used kernels
struct ConvertRawSamplesShaderArgs
{
	uint32_t size;
	float gain;
	float offset;
};

//Vulkan global stuff
extern vk::raii::Context g_vkContext;
extern std::unique_ptr<vk::raii::Instance> g_vkInstance;
extern uint8_t g_vkComputeDeviceUuid[16];
extern uint32_t g_vkComputeDeviceDriverVer;
extern vk::raii::PhysicalDevice* g_vkComputePhysicalDevice;

void SubmitAndBlock(vk::raii::CommandBuffer& cmdBuf, vk::raii::Queue& queue);

//Enable flags for various features
extern bool g_gpuFilterEnabled;
extern bool g_gpuScopeDriverEnabled;
extern bool g_hasShaderInt64;
extern bool g_hasShaderInt16;
extern bool g_hasShaderInt8;
extern bool g_hasDebugUtils;

extern uint32_t g_computeQueueType;
extern uint32_t g_renderQueueType;

int AllocateVulkanComputeQueue();
int AllocateVulkanRenderQueue();

uint32_t GetComputeBlockCount(size_t numGlobal, size_t blockSize);

#endif