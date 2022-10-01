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
	@brief Main library include file
 */

#ifndef scopehal_h
#define scopehal_h

#define __USE_MINGW_ANSI_STDIO 1 // Required for MSYS2 mingw64 to support format "%z" ..

#include <vector>
#include <string>
#include <map>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <memory>

#include <sigc++/sigc++.h>
#include <cairomm/context.h>

#include <yaml-cpp/yaml.h>

#include "../log/log.h"
#include "../graphwidget/Graph.h"

#include "config.h"

//Vulkan is now a mandatory dependency, so no compile time enable flag
//(disable some warnings in Vulkan headers that we can't do anything about)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <vulkan/vulkan_raii.hpp>
#pragma GCC diagnostic pop

//must be early because a lot of inline methods in headers rely on these
#ifdef __x86_64__
extern bool g_hasFMA;
extern bool g_hasAvx512F;
extern bool g_hasAvx512VL;
extern bool g_hasAvx512DQ;
extern bool g_hasAvx2;
#endif

#include "Unit.h"
#include "Bijection.h"
#include "IDTable.h"

#include "AcceleratorBuffer.h"
#include "ComputePipeline.h"

#include "SCPITransport.h"
#include "SCPISocketTransport.h"
#include "SCPITwinLanTransport.h"
#include "SCPILinuxGPIBTransport.h"
#include "SCPILxiTransport.h"
#include "SCPINullTransport.h"
#include "SCPITMCTransport.h"
#include "SCPIUARTTransport.h"
#include "VICPSocketTransport.h"
#include "SCPIDevice.h"

#include "FlowGraphNode.h"
#include "OscilloscopeChannel.h"
#include "Trigger.h"

#include "Instrument.h"
#include "FunctionGenerator.h"
#include "Multimeter.h"
#include "Oscilloscope.h"
#include "SParameterChannel.h"
#include "PowerSupply.h"
#include "RFSignalGenerator.h"
#include "SCPIInstrument.h"
#include "SCPIFunctionGenerator.h"
#include "SCPIMultimeter.h"
#include "SCPIOscilloscope.h"
#include "SCPIRFSignalGenerator.h"
#include "SCPIPowerSupply.h"

#include "SParameters.h"
#include "TouchstoneParser.h"
#include "IBISParser.h"

#include "Statistic.h"
#include "FilterParameter.h"
#include "Filter.h"
#include "ImportFilter.h"
#include "PeakDetectionFilter.h"
#include "SpectrumChannel.h"
#include "SParameterSourceFilter.h"
#include "SParameterFilter.h"

#include "ExportWizard.h"
#include "FilterGraphExecutor.h"

#include "ScopehalUtils.h"
#include "ScopehalVulkanUtils.h"

uint64_t ConvertVectorSignalToScalar(const std::vector<bool>& bits);

void TransportStaticInit();
void DriverStaticInit();

bool VulkanInit(bool skipGLFW = false);
void InitializeSearchPaths();
void InitializePlugins();
void DetectCPUFeatures();
std::string GetDirOfCurrentExecutable();

void ScopehalStaticCleanup();

//Validation helper for templates
//Throws compile-time error if specialized for false since there's no implementation
template<bool> class CompileTimeAssert;
template<> class CompileTimeAssert<true>{};

#endif
