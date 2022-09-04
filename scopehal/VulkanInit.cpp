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
	@brief Vulkan initialization
 */
#include "scopehal.h"
#include <glslang_c_interface.h>

//Lots of warnings here, disable them
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <vkFFT.h>
#pragma GCC diagnostic pop

using namespace std;

/**
	@brief Global Vulkan context
 */
vk::raii::Context g_vkContext;

/**
	@brief Global Vulkan instance
 */
unique_ptr<vk::raii::Instance> g_vkInstance;

/**
	@brief The Vulkan device selected for compute operations (may or may not be same device as rendering)
 */
unique_ptr<vk::raii::Device> g_vkComputeDevice;

/**
	@brief Command pool for AcceleratorBuffer transfers

	This is a single global resource interlocked by g_vkTransferMutex and is used for convenience and code simplicity
	when parallelism isn't that important.
 */
unique_ptr<vk::raii::CommandPool> g_vkTransferCommandPool;

/**
	@brief Command buffer for AcceleratorBuffer transfers

	This is a single global resource interlocked by g_vkTransferMutex and is used for convenience and code simplicity
	when parallelism isn't that important.
 */
unique_ptr<vk::raii::CommandBuffer> g_vkTransferCommandBuffer;

/**
	@brief Queue for AcceleratorBuffer transfers

	This is a single global resource interlocked by g_vkTransferMutex and is used for convenience and code simplicity
	when parallelism isn't that important.
 */
unique_ptr<vk::raii::Queue> g_vkTransferQueue;

/**
	@brief Mutex for interlocking access to g_vkTransferCommandBuffer and g_vkTransferCommandPool
 */
mutex g_vkTransferMutex;

/**
	@brief Vulkan memory type for CPU-based memory that is also GPU-readable
 */
size_t g_vkPinnedMemoryType;

/**
	@brief Vulkan memory type for GPU-based memory (generally not CPU-readable, except on integrated cards)
 */
size_t g_vkLocalMemoryType;

/**
	@brief Vulkan queue type for submitting compute operations (may or may not be render capable)
 */
size_t g_computeQueueType;

/**
	@brief Command buffer for submitting vkFFT calls to
 */
unique_ptr<vk::raii::CommandPool> g_vkFFTCommandPool;

/**
	@brief Command buffer for submitting vkFFT calls to
 */
unique_ptr<vk::raii::CommandBuffer> g_vkFFTCommandBuffer;

/**
	@brief Command queue for submitting vkFFT calls to
 */
unique_ptr<vk::raii::Queue> g_vkFFTQueue;

/**
	@brief Mutex for controlling access to g_vkfFFT*
 */
mutex g_vkFFTMutex;

bool IsDevicePreferred(const vk::PhysicalDeviceProperties& a, const vk::PhysicalDeviceProperties& b);

//Feature flags indicating that we have support for specific data types etc on the GPU
bool g_hasShaderInt64 = false;
bool g_hasShaderInt16 = false;
bool g_hasShaderInt8 = false;

void VulkanCleanup();

/**
	@brief vkFFT is weird and needs to hold onto the *physical* device...
 */
vk::raii::PhysicalDevice* g_vkfftPhysicalDevice;

/**
	@brief Allocates a queue index for Vulkan compute queues
 */
int AllocateVulkanComputeQueue()
{
	static mutex allocMutex;

	lock_guard<mutex> lock(allocMutex);

	static int nextQueue = 0;
	return (nextQueue ++);
}

/**
	@brief Initialize a Vulkan context for compute
 */
bool VulkanInit()
{
	LogDebug("Initializing Vulkan\n");
	LogIndenter li;

	try
	{
		auto extensions = g_vkContext.enumerateInstanceExtensionProperties();
		bool hasPhysicalDeviceProperties2 = false;
		for(auto e : extensions)
		{
			if(!strcmp((char*)e.extensionName, "VK_KHR_get_physical_device_properties2"))
			{
				LogDebug("VK_KHR_get_physical_device_properties2: supported\n");
				hasPhysicalDeviceProperties2 = true;
			}
		}

		//Vulkan 1.1 is the highest version supported on all targeted platforms (limited mostly by MoltenVK)
		//But if Vulkan 1.2 is available, request it.
		//TODO: If we want to support llvmpipe, we need to stick to 1.0
		auto apiVersion = VK_API_VERSION_1_1;
		auto availableVersion = g_vkContext.enumerateInstanceVersion();
		uint32_t loader_major = VK_VERSION_MAJOR(availableVersion);
		uint32_t loader_minor = VK_VERSION_MINOR(availableVersion);
		bool vulkan12Available = false;
		LogDebug("Loader/API support available for Vulkan %d.%d\n", loader_major, loader_minor);
		if( (loader_major >= 1) || ( (loader_major == 1) && (loader_minor >= 2) ) )
		{
			apiVersion = VK_API_VERSION_1_2;
			vulkan12Available = true;
			LogDebug("Vulkan 1.2 support available, requesting it\n");
		}
		else
			LogDebug("Vulkan 1.2 support not available\n");

		//Request VK_KHR_get_physical_device_properties2 if available
		vk::ApplicationInfo appInfo("libscopehal", 1, "Vulkan.hpp", 1, apiVersion);
		vector<const char*> extensionsToUse;
		if(hasPhysicalDeviceProperties2)
			extensionsToUse.push_back("VK_KHR_get_physical_device_properties2");
		vk::InstanceCreateInfo instanceInfo({}, &appInfo, {}, extensionsToUse);

		//Create the instance
		g_vkInstance = make_unique<vk::raii::Instance>(g_vkContext, instanceInfo);

		//Look at our physical devices and print info out for each one
		LogDebug("Physical devices:\n");
		{
			LogIndenter li2;

			size_t bestDevice = 0;

			static vk::raii::PhysicalDevices devices(*g_vkInstance);
			for(size_t i=0; i<devices.size(); i++)
			{
				auto device = devices[i];
				auto features = device.getFeatures();
				auto properties = device.getProperties();
				auto memProperties = device.getMemoryProperties();
				auto limits = properties.limits;

				//See what device to use
				//TODO: preference to override this
				if(IsDevicePreferred(devices[bestDevice].getProperties(), devices[i].getProperties()))
					bestDevice = i;

				//TODO: sparse properties

				LogDebug("Device %zu: %s\n", i, &properties.deviceName[0]);
				LogIndenter li3;

				LogDebug("API version:            0x%08x (%d.%d.%d.%d)\n",
					properties.apiVersion,
					(properties.apiVersion >> 29),
					(properties.apiVersion >> 22) & 0x7f,
					(properties.apiVersion >> 12) & 0x3ff,
					(properties.apiVersion >> 0) & 0xfff
					);

				//Driver version is NOT guaranteed to be encoded the same way as the API version.
				if(properties.vendorID == 0x10de)	//NVIDIA
				{
					LogDebug("Driver version:         0x%08x (%d.%d.%d.%d)\n",
						properties.driverVersion,
						(properties.driverVersion >> 22),
						(properties.driverVersion >> 14) & 0xff,
						(properties.driverVersion >> 6) & 0xff,
						(properties.driverVersion >> 0) & 0x3f
						);
				}

				//By default, assume it's the same as API
				else
				{
					LogDebug("Driver version:         0x%08x (%d.%d.%d.%d)\n",
						properties.driverVersion,
						(properties.driverVersion >> 29),
						(properties.driverVersion >> 22) & 0x7f,
						(properties.driverVersion >> 12) & 0x3ff,
						(properties.driverVersion >> 0) & 0xfff
						);
				}

				LogDebug("Vendor ID:              %04x\n", properties.vendorID);
				LogDebug("Device ID:              %04x\n", properties.deviceID);
				switch(properties.deviceType)
				{
					case vk::PhysicalDeviceType::eIntegratedGpu:
						LogDebug("Device type:            Integrated GPU\n");
						break;

					case vk::PhysicalDeviceType::eDiscreteGpu:
						LogDebug("Device type:            Discrete GPU\n");
						break;

					case vk::PhysicalDeviceType::eVirtualGpu:
						LogDebug("Device type:            Virtual GPU\n");
						break;

					case vk::PhysicalDeviceType::eCpu:
						LogDebug("Device type:            CPU\n");
						break;

					default:
					case vk::PhysicalDeviceType::eOther:
						LogDebug("Device type:            Other\n");
						break;
				}

				if(features.shaderInt64)
					LogDebug("int64:                  yes\n");
				else
					LogDebug("int64:                  no\n");

				if(hasPhysicalDeviceProperties2)
				{
					//Get more details
					auto features2 = device.getFeatures2<
						vk::PhysicalDeviceFeatures2,
						vk::PhysicalDevice16BitStorageFeatures,
						vk::PhysicalDevice8BitStorageFeatures,
						vk::PhysicalDeviceVulkan12Features
						>();
					auto storageFeatures16 = std::get<1>(features2);
					auto storageFeatures8 = std::get<2>(features2);
					auto vulkan12Features = std::get<3>(features2);

					if(features.shaderInt16)
					{
						if(storageFeatures16.storageBuffer16BitAccess)
							LogDebug("int16:                  yes (allowed in SSBOs)\n");
						else
							LogDebug("int16:                  yes (but not allowed in SSBOs)\n");
					}
					else
						LogDebug("int16:                  no\n");

					if(vulkan12Features.shaderInt8)
					{
						if(storageFeatures8.uniformAndStorageBuffer8BitAccess)
							LogDebug("int8:                   yes (allowed in SSBOs)\n");
						else
							LogDebug("int8:                   yes (but not allowed in SSBOs)\n");
					}
					else
						LogDebug("int8:                   no\n");
				}

				const size_t k = 1024LL;
				const size_t m = k*k;
				const size_t g = k*m;

				LogDebug("Max image dim 2D:       %u\n", limits.maxImageDimension2D);
				LogDebug("Max storage buf range:  %lu MB\n", limits.maxStorageBufferRange / m);
				LogDebug("Max mem alloc:          %lu MB\n", limits.maxMemoryAllocationCount / m);
				LogDebug("Max compute shared mem: %lu KB\n", limits.maxComputeSharedMemorySize / k);
				LogDebug("Max compute grp count:  %u x %u x %u\n",
					limits.maxComputeWorkGroupCount[0],
					limits.maxComputeWorkGroupCount[1],
					limits.maxComputeWorkGroupCount[2]);
				LogDebug("Max compute invocs:     %u\n", limits.maxComputeWorkGroupInvocations);
				LogDebug("Max compute grp size:   %u x %u x %u\n",
					limits.maxComputeWorkGroupSize[0],
					limits.maxComputeWorkGroupSize[1],
					limits.maxComputeWorkGroupSize[2]);

				LogDebug("Memory types:\n");
				for(size_t j=0; j<memProperties.memoryTypeCount; j++)
				{
					auto mtype = memProperties.memoryTypes[j];

					LogIndenter li4;
					LogDebug("Type %zu\n", j);
					LogIndenter li5;

					LogDebug("Heap index: %u\n", mtype.heapIndex);
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
						LogDebug("Device local\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
						LogDebug("Host visible\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)
						LogDebug("Host coherent\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostCached)
						LogDebug("Host cached\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eLazilyAllocated)
						LogDebug("Lazily allocated\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eProtected)
						LogDebug("Protected\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceCoherentAMD)
						LogDebug("Device coherent\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceUncachedAMD)
						LogDebug("Device uncached\n");
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eRdmaCapableNV)
						LogDebug("RDMA capable\n");
				}

				LogDebug("Memory heaps:\n");
				for(size_t j=0; j<memProperties.memoryHeapCount; j++)
				{
					LogIndenter li4;
					LogDebug("Heap %zu\n", j);
					LogIndenter li5;
					auto heap = memProperties.memoryHeaps[j];

					if(heap.size > g)
						LogDebug("Size: %zu GB\n", heap.size / g);
					else if(heap.size > m)
						LogDebug("Size: %zu MB\n", heap.size / m);
					else if(heap.size > k)
						LogDebug("Size: %zu kB\n", heap.size / k);
					else
						LogDebug("Size: %zu B\n", heap.size);

					if(heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
						LogDebug("Device local\n");
					if(heap.flags & vk::MemoryHeapFlagBits::eMultiInstance)
						LogDebug("Multi instance\n");
					if(heap.flags & vk::MemoryHeapFlagBits::eMultiInstanceKHR)
						LogDebug("Multi instance (KHR)\n");
				}
			}

			LogDebug("Selected device %zu\n", bestDevice);
			int computeQueueCount = 1;
			{
				auto device = devices[bestDevice];
				g_vkfftPhysicalDevice = &devices[bestDevice];

				LogIndenter li3;

				//Look at queue families
				auto families = device.getQueueFamilyProperties();
				LogDebug("Queue families\n");
				{
					LogIndenter li4;
					g_computeQueueType = 0;
					for(size_t j=0; j<families.size(); j++)
					{
						LogDebug("Queue type %zu\n", j);
						LogIndenter li5;

						auto f = families[j];
						LogDebug("Queue count:          %d\n", f.queueCount);
						LogDebug("Timestamp valid bits: %d\n", f.timestampValidBits);
						if(f.queueFlags & vk::QueueFlagBits::eGraphics)
							LogDebug("Graphics\n");
						if(f.queueFlags & vk::QueueFlagBits::eCompute)
							LogDebug("Compute\n");
						if(f.queueFlags & vk::QueueFlagBits::eTransfer)
							LogDebug("Transfer\n");
						if(f.queueFlags & vk::QueueFlagBits::eSparseBinding)
							LogDebug("Sparse binding\n");
						if(f.queueFlags & vk::QueueFlagBits::eProtected)
							LogDebug("Protected\n");
						//TODO: VIDEO_DECODE_BIT_KHR, VIDEO_ENCODE_BIT_KHR

						//Pick the first type that supports compute and transfers
						if( (f.queueFlags & vk::QueueFlagBits::eCompute) && (f.queueFlags & vk::QueueFlagBits::eTransfer) )
						{
							g_computeQueueType = j;
							computeQueueCount = f.queueCount;
							break;
						}
					}
				}

				//See if the device has good integer data type support. If so, enable it
				vk::PhysicalDeviceFeatures enabledFeatures;
				vk::PhysicalDevice16BitStorageFeatures features16bit;
				vk::PhysicalDevice8BitStorageFeatures features8bit;
				vk::PhysicalDeviceVulkan12Features featuresVulkan12;
				void* pNext = nullptr;
				if(device.getFeatures().shaderInt64)
				{
					enabledFeatures.shaderInt64 = true;
					g_hasShaderInt64 = true;
					LogDebug("Enabling 64-bit integer support\n");
				}
				if(device.getFeatures().shaderInt16)
				{
					enabledFeatures.shaderInt16 = true;
					LogDebug("Enabling 16-bit integer support\n");
				}
				if(hasPhysicalDeviceProperties2)
				{
					//Get more details
					auto features2 = device.getFeatures2<
						vk::PhysicalDeviceFeatures2,
						vk::PhysicalDevice16BitStorageFeatures,
						vk::PhysicalDevice8BitStorageFeatures,
						vk::PhysicalDeviceVulkan12Features
						>();
					auto storageFeatures16 = std::get<1>(features2);
					auto storageFeatures8 = std::get<2>(features2);
					auto vulkan12Features = std::get<3>(features2);

					//Enable 16 bit SSBOs
					if(storageFeatures16.storageBuffer16BitAccess)
					{
						features16bit.storageBuffer16BitAccess = true;
						features16bit.pNext = pNext;
						pNext = &features16bit;
						LogDebug("Enabling 16-bit integer support for SSBOs\n");
						g_hasShaderInt16 = true;
					}

					//Vulkan 1.2 allows some stuff to be done simpler
					if(vulkan12Available)
					{
						if(storageFeatures16.storageBuffer16BitAccess)

						//Enable 8 bit shader variables
						if(vulkan12Features.shaderInt8)
						{
							featuresVulkan12.shaderInt8 = true;
							LogDebug("Enabling 8-bit integer support\n");
						}

						//Enable 8 bit SSBOs
						if(storageFeatures8.uniformAndStorageBuffer8BitAccess)
						{
							featuresVulkan12.uniformAndStorageBuffer8BitAccess = true;
							LogDebug("Enabling 8-bit integer support for SSBOs\n");
							g_hasShaderInt8 = true;
						}

						featuresVulkan12.pNext = pNext;
						pNext = &featuresVulkan12;
					}

					//Nope, need to use the old way
					else
					{
						//Enable 8 bit SSBOs
						if(storageFeatures8.storageBuffer8BitAccess)
						{
							features8bit.storageBuffer8BitAccess = true;
							features8bit.pNext = pNext;
							pNext = &features8bit;
							LogDebug("Enabling 8-bit integer support for SSBOs\n");
						}
					}
				}

				//Initialize the device
				//Create as many compute queues as we're allowed to, and make them all equal priority.
				vector<float> queuePriority;
				for(int i=0; i<computeQueueCount; i++)
					queuePriority.push_back(0.5);
				vk::DeviceQueueCreateInfo qinfo( {}, g_computeQueueType, computeQueueCount, &queuePriority[0]);
				vk::DeviceCreateInfo devinfo(
					{},
					qinfo,
					{},
					{},
					&enabledFeatures,
					pNext);
				g_vkComputeDevice = make_unique<vk::raii::Device>(device, devinfo);

				//Figure out what memory types to use for various purposes
				bool foundPinnedType = false;
				bool foundLocalType = false;
				g_vkPinnedMemoryType = 0;
				g_vkLocalMemoryType = 0;
				auto memProperties = device.getMemoryProperties();
				auto devtype = device.getProperties().deviceType;
				for(size_t j=0; j<memProperties.memoryTypeCount; j++)
				{
					auto mtype = memProperties.memoryTypes[j];

					//Pinned memory is host visible, host coherent, host cached, and usually not device local
					//Use the first type we find
					if(
						(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) &&
						(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) &&
						(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostCached) )
					{
						//Device local? This is a disqualifier UNLESS we are an integrated card or CPU
						//(in which case we have shared memory)
						if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
						{
							if( (devtype != vk::PhysicalDeviceType::eIntegratedGpu) &&
								(devtype != vk::PhysicalDeviceType::eCpu ) )
							{
								continue;
							}
						}

						if(!foundPinnedType)
						{
							foundPinnedType = true;
							g_vkPinnedMemoryType = j;
						}
					}

					//Local memory is device local
					//Use the first type we find
					if(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
					{
						//Exclude any types that are host visible unless we're an integrated card
						//(Host visible + device local memory is generally limited and
						if( (devtype != vk::PhysicalDeviceType::eIntegratedGpu) &&
							(mtype.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) )
						{
							continue;
						}

						if(!foundLocalType)
						{
							foundLocalType = true;
							g_vkLocalMemoryType = j;
						}
					}
				}

				LogDebug("Using type %zu for pinned host memory\n", g_vkPinnedMemoryType);
				LogDebug("Using type %zu for card-local memory\n", g_vkLocalMemoryType);

				//Make a CommandPool for transfers and another one for vkFFT
				vk::CommandPoolCreateInfo poolInfo(
					vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
					g_computeQueueType );
				g_vkTransferCommandPool = make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);
				g_vkFFTCommandPool = make_unique<vk::raii::CommandPool>(*g_vkComputeDevice, poolInfo);

				//Make a CommandBuffer for memory transfers that we can use implicitly during buffer management
				vk::CommandBufferAllocateInfo bufinfo(**g_vkTransferCommandPool, vk::CommandBufferLevel::ePrimary, 1);
				g_vkTransferCommandBuffer = make_unique<vk::raii::CommandBuffer>(
					std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));

				//Make a Queue for memory transfers that we can use implicitly during buffer management
				g_vkTransferQueue = make_unique<vk::raii::Queue>(
					*g_vkComputeDevice, g_computeQueueType, AllocateVulkanComputeQueue());

				//And again for FFTs
				bufinfo = vk::CommandBufferAllocateInfo(**g_vkFFTCommandPool, vk::CommandBufferLevel::ePrimary, 1);
				g_vkFFTCommandBuffer = make_unique<vk::raii::CommandBuffer>(
					std::move(vk::raii::CommandBuffers(*g_vkComputeDevice, bufinfo).front()));
				g_vkFFTQueue = make_unique<vk::raii::Queue>(
					*g_vkComputeDevice, g_computeQueueType, AllocateVulkanComputeQueue());
			}
		}
	}
	catch ( vk::SystemError & err )
	{
		LogError("vk::SystemError: %s\n", err.what());
		return false;
	}
	catch ( std::exception & err )
	{
		LogError("std::exception: %s\n", err.what());
		return false;
	}
	catch(...)
	{
		LogError("unknown exception\n");
		return false;
	}

	LogDebug("\n");

	//If we get here, everything is good
	g_gpuFilterEnabled = true;
	g_gpuScopeDriverEnabled = true;

	//Initialize the glsl compiler since vkFFT does JIT generation of kernels
	if(1 != glslang_initialize_process())
		LogError("Failed to initialize glslang compiler\n");

	//Print out vkFFT version for debugging
	int vkfftver = VkFFTGetVersion();
	int vkfft_major = vkfftver / 10000;
	int vkfft_minor = (vkfftver / 100) % 100;
	int vkfft_patch = vkfftver % 100;
	LogDebug("vkFFT version: %d.%d.%d\n", vkfft_major, vkfft_minor, vkfft_patch);

	return true;
}

/**
	@brief Checks if a given Vulkan device is "better" than another

	True if we should use device B over A
 */
bool IsDevicePreferred(const vk::PhysicalDeviceProperties& a, const vk::PhysicalDeviceProperties& b)
{
	//If B is a discrete GPU, always prefer it
	//TODO: prefer one of multiple
	if(b.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		return true;

	//Integrated GPUs beat anything but a discrete GPU
	if( (b.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) &&
		(a.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) )
	{
		return true;
	}

	//Anything is better than a CPU
	if(a.deviceType == vk::PhysicalDeviceType::eCpu)
		return false;

	//By default, assume A is good enough
	return false;
}

void VulkanCleanup()
{
	glslang_finalize_process();

	g_vkFFTQueue = nullptr;
	g_vkFFTCommandBuffer = nullptr;
	g_vkFFTCommandPool = nullptr;

	g_vkTransferQueue = nullptr;
	g_vkTransferCommandBuffer = nullptr;
	g_vkTransferCommandPool = nullptr;

	g_vkComputeDevice = nullptr;
	g_vkInstance = nullptr;
}
