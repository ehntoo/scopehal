/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of VulkanFFTPlan
 */
#include "VulkanFFTPlan.h"
#include "ScopehalVulkanUtils.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

VulkanFFTPlan::VulkanFFTPlan(size_t npoints, size_t nouts, VulkanFFTPlanDirection dir)
	: m_size(npoints)
	, m_fence(*g_vkComputeDevice, vk::FenceCreateInfo())
{
	memset(&m_app, 0, sizeof(m_app));
	memset(&m_config, 0, sizeof(m_config));

	//Create a command pool for initialization use
	vk::CommandPoolCreateInfo poolInfo(
		vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		g_computeQueueType );
	vk::raii::CommandPool pool(*g_vkComputeDevice, poolInfo);

	//Only 1D FFTs supported for now
	m_config.FFTdim = 1;
	m_config.size[0] = npoints;
	m_config.size[1] = 1;
	m_config.size[2] = 1;

	string cacheKey;
	if(dir == DIRECTION_FORWARD)
	{
		m_config.makeForwardPlanOnly = 1;

		//output is complex buffer of full size
		m_bsize = 2 * nouts * sizeof(float);

		//input is real buffer of full size
		m_isize = npoints * sizeof(float);

		m_config.bufferSize = &m_bsize;
		m_config.inputBufferSize = &m_isize;

		cacheKey = string("VkFFT_FWD_") + to_string(npoints);
	}
	else
	{
		m_config.makeInversePlanOnly = 1;

		//input is complex buffer of full size
		m_isize = 2 * nouts * sizeof(float);

		//output is real buffer of full size
		m_bsize = npoints * sizeof(float);

		m_config.bufferSize = &m_bsize;
		m_config.inputBufferSize = &m_isize;
		m_config.inverseReturnToInputBuffer = 1;

		cacheKey = string("VkFFT_INV_") + to_string(npoints);
	}

	lock_guard<mutex> lock(g_vkTransferMutex);

	//Extract raw handles of all of our Vulkan objects
	m_physicalDevice = **g_vkComputePhysicalDevice;
	m_device = **g_vkComputeDevice;
	VkCommandPool rpool = *pool;
	VkQueue queue = **g_vkTransferQueue;
	m_rawfence = *m_fence;
	m_pipelineCache = **g_pipelineCacheMgr->Lookup(cacheKey + ".spv", VkFFTGetVersion());

	if(g_hasDebugUtils)
	{
		g_vkComputeDevice->setDebugUtilsObjectNameEXT(
			vk::DebugUtilsObjectNameInfoEXT(
				vk::ObjectType::eFence,
				reinterpret_cast<int64_t>(m_rawfence),
				"VulkanFFTPlan.m_rawfence"));
	}

	m_config.physicalDevice = &m_physicalDevice;
	m_config.device = &m_device;
	m_config.queue = &queue;
	m_config.commandPool = &rpool;
	m_config.fence = &m_rawfence;
	m_config.isCompilerInitialized = 1;
	m_config.isInputFormatted = 1;
	m_config.pipelineCache = &m_pipelineCache;

	//We have "C" locale all the time internally, so no need to setlocale() in the library
	m_config.disableSetLocale = 1;

	m_config.performR2C = 1;				//real time domain / complex frequency domain points

	//Load from cache
	auto cacheBlob = g_pipelineCacheMgr->LookupRaw(cacheKey);
	if(cacheBlob != nullptr)
	{
		m_config.loadApplicationFromString = 1;
		m_config.loadApplicationString = &(*cacheBlob)[0];
	}

	//If not in cache, tell the library to generate a cache entry this time so we don't have to do it again
	else
		m_config.saveApplicationToString = 1;

	auto err = initializeVkFFT(&m_app, m_config);
	if(VKFFT_SUCCESS != err)
		LogError("Failed to initialize vkFFT (code %d)\n", err);

	//Add to cache if it wasn't there already
	if(cacheBlob == nullptr)
	{
		auto vec = make_shared<vector<uint8_t> >();
		vec->resize(m_app.applicationStringSize);
		memcpy(&(*vec)[0], m_app.saveApplicationString, m_app.applicationStringSize);
		g_pipelineCacheMgr->StoreRaw(cacheKey, vec);
	}

	//Done initializing, clear queue pointers to make sure nothing uses it
	m_config.queue = VK_NULL_HANDLE;
	m_config.commandPool = VK_NULL_HANDLE;
}

VulkanFFTPlan::~VulkanFFTPlan()
{
	deleteVkFFT(&m_app);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execution

void VulkanFFTPlan::AppendForward(
	AcceleratorBuffer<float>& dataIn,
	AcceleratorBuffer<float>& dataOut,
	vk::raii::CommandBuffer& cmdBuf)
{
	dataIn.PrepareForGpuAccess();
	dataOut.PrepareForGpuAccess();

	//Extract raw handles of all of our Vulkan objects
	VkBuffer inbuf = dataIn.GetBuffer();
	VkBuffer outbuf = dataOut.GetBuffer();
	VkCommandBuffer cmd = *cmdBuf;

	VkFFTLaunchParams params;
	memset(&params, 0, sizeof(params));
	params.inputBuffer = &inbuf;
	params.buffer = &outbuf;
	params.commandBuffer = &cmd;

	auto err = VkFFTAppend(&m_app, -1, &params);
	if(VKFFT_SUCCESS != err)
		LogError("Failed to append vkFFT transform (code %d)\n", err);

	dataOut.MarkModifiedFromGpu();
}

void VulkanFFTPlan::AppendReverse(
	AcceleratorBuffer<float>& dataIn,
	AcceleratorBuffer<float>& dataOut,
	vk::raii::CommandBuffer& cmdBuf)
{
	dataIn.PrepareForGpuAccess();
	dataOut.PrepareForGpuAccess();

	//Extract raw handles of all of our Vulkan objects
	VkBuffer inbuf = dataIn.GetBuffer();
	VkBuffer outbuf = dataOut.GetBuffer();
	VkCommandBuffer cmd = *cmdBuf;

	VkFFTLaunchParams params;
	memset(&params, 0, sizeof(params));
	params.inputBuffer = &outbuf;		//inverse transform writes to *input* buffer when returnToInputBuffer is set
	params.buffer = &inbuf;
	params.commandBuffer = &cmd;

	auto err = VkFFTAppend(&m_app, 1, &params);
	if(VKFFT_SUCCESS != err)
		LogError("Failed to append vkFFT transform (code %d)\n", err);

	dataOut.MarkModifiedFromGpu();
}
