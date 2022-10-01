#include "ScopehalVulkanUtils.h"
#include "AcceleratorBuffer.h"

///@brief True if filters can use GPU acceleration
bool g_gpuFilterEnabled = false;

///@brief True if scope drivers can use GPU acceleration
bool g_gpuScopeDriverEnabled = false;

/**
	@brief Helper function that submits a command buffer to a queue and blocks until it completes
 */
void SubmitAndBlock(vk::raii::CommandBuffer& cmdBuf, vk::raii::Queue& queue)
{
	vk::raii::Fence fence(*g_vkComputeDevice, vk::FenceCreateInfo());
	vk::SubmitInfo info({}, {}, *cmdBuf);
	queue.submit(info, *fence);
	while(vk::Result::eTimeout == g_vkComputeDevice->waitForFences({*fence}, VK_TRUE, 1000 * 1000))
	{}
}

uint32_t GetComputeBlockCount(size_t numGlobal, size_t blockSize)
{
	uint32_t ret = numGlobal / blockSize;
	if(numGlobal % blockSize)
		ret ++;
	return ret;
}
