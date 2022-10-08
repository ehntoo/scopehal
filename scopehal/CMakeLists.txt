find_library(LXI_LIB lxi)
find_library(LINUXGPIB_LIB gpib)
find_package(glfw3 REQUIRED)

find_package(yaml-cpp REQUIRED)
find_package(PkgConfig)
pkg_check_modules(cairomm REQUIRED IMPORTED_TARGET cairomm-1.0)
pkg_check_modules(SIGCXX REQUIRED IMPORTED_TARGET sigc++-2.0)

find_package(OpenMP REQUIRED)
# Configure and enable OpenMP
if(NOT OpenMP_CXX_FOUND)
	message(FATAL_ERROR "scopehal requires OpenMP but your C++ compiler does not appear to support it.")
endif()


# Additional Windows/Linux libraries
if(WIN32)
	set(WIN_LIBS shlwapi)
else()
	set(LIN_LIBS dl)
endif()

# Apple has their own idea about stat structs
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dst_mtim=st_mtimespec")
endif()

if(LXI_LIB)
	set(HAS_LXI true)
	set(LXI_LIBRARIES ${LXI_LIB})
else()
	set(HAS_LXI false)
	set(LXI_LIBRARIES "")
endif()

if(LINUXGPIB_LIB)
	set(HAS_LINUXGPIB true)
	set(LINUXGPIB_LIBRARIES ${LINUXGPIB_LIB})
else()
	set(HAS_LINUXGPIB false)
	set(LINUXGPIB_LIBRARIES "")
endif()

set(SCOPEHAL_SOURCES
	base64.cpp
	scopehal.cpp
	ScopehalUtil.cpp
	avx_mathfun.cpp
	VulkanInit.cpp

	FileSystem.cpp
	Unit.cpp
	Waveform.cpp

	SCPITransport.cpp
	SCPISocketTransport.cpp
	SCPITwinLanTransport.cpp
	VICPSocketTransport.cpp
	SCPILinuxGPIBTransport.cpp
	SCPILxiTransport.cpp
	SCPINullTransport.cpp
	SCPIUARTTransport.cpp
	SCPIDevice.cpp

	IBISParser.cpp
	SParameters.cpp
	TouchstoneParser.cpp

	FlowGraphNode.cpp
	Trigger.cpp
	CDRTrigger.cpp
	CDR8B10BTrigger.cpp
	CDRNRZPatternTrigger.cpp
	DCAEdgeTrigger.cpp
	DropoutTrigger.cpp
	EdgeTrigger.cpp
	GlitchTrigger.cpp
	NthEdgeBurstTrigger.cpp
	PulseWidthTrigger.cpp
	RuntTrigger.cpp
	SerialTrigger.cpp
	SlewRateTrigger.cpp
	TwoLevelTrigger.cpp
	UartTrigger.cpp
	WindowTrigger.cpp

	Instrument.cpp
	FunctionGenerator.cpp
	Multimeter.cpp
	Oscilloscope.cpp
	OscilloscopeChannel.cpp
	PowerSupply.cpp
	RFSignalGenerator.cpp
	SCPIFunctionGenerator.cpp
	SCPIInstrument.cpp
	SCPIMultimeter.cpp
	SCPIOscilloscope.cpp
	SCPIPowerSupply.cpp
	SCPIRFSignalGenerator.cpp
	RemoteBridgeOscilloscope.cpp

	AgilentOscilloscope.cpp
	AntikernelLabsOscilloscope.cpp
	AntikernelLogicAnalyzer.cpp
	DemoOscilloscope.cpp
	DigilentOscilloscope.cpp
	DSLabsOscilloscope.cpp
	GWInstekGPDX303SPowerSupply.cpp
	KeysightDCA.cpp
	LeCroyOscilloscope.cpp
	MockOscilloscope.cpp
	PicoOscilloscope.cpp
	RigolOscilloscope.cpp
	SiglentSCPIOscilloscope.cpp
	SiglentVectorSignalGenerator.cpp
	TektronixOscilloscope.cpp
	RohdeSchwarzOscilloscope.cpp
	RohdeSchwarzHMC8012Multimeter.cpp
	RohdeSchwarzHMC804xPowerSupply.cpp

	StandardColors.cpp
	Filter.cpp
	FilterParameter.cpp
	ImportFilter.cpp
	PacketDecoder.cpp
	PeakDetectionFilter.cpp
	Statistic.cpp
	SpectrumChannel.cpp
	SParameterSourceFilter.cpp
	SParameterFilter.cpp

	TestWaveformSource.cpp

	ComputePipeline.cpp
	FilterGraphExecutor.cpp
	PipelineCacheManager.cpp
	VulkanFFTPlan.cpp
	)

if(NOT WIN32 AND NOT APPLE)
	list(APPEND SCOPEHAL_SOURCES
		# TMC is only supported on Linux for now.
		# https://github.com/glscopeclient/scopehal/issues/519
		SCPITMCTransport.cpp
	)
endif()

configure_file(config.h.in config.h)

add_library(scopehal SHARED
	${SCOPEHAL_SOURCES})

target_link_libraries(scopehal
	PkgConfig::SIGCXX
	PkgConfig::cairomm
	xptools
	log
	yaml-cpp
	${LXI_LIBRARIES}
	${LINUXGPIB_LIBRARIES}
	${WIN_LIBS}
	${LIN_LIBS}
	${LIBFFTS_LIBRARIES}
	OpenMP::OpenMP_CXX
	Vulkan::Vulkan
	Vulkan::glslang
	Vulkan::shaderc_combined
	glfw
	)

target_include_directories(scopehal
PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${LIBFFTS_INCLUDE_DIR}

	${Vulkan_INCLUDE_DIR}/glslang/Include
	# TODO: this needs to come from FindPackage etc
	/usr/include/glslang/Include
	# for macOS:
	/usr/local/include/glslang/Include
	)

target_include_directories(scopehal
PUBLIC SYSTEM
	${CMAKE_CURRENT_SOURCE_DIR}/../VkFFT/vkFFT
	)

target_precompile_headers(scopehal
PUBLIC

	# libstdc++ headers used by vulkan and/or very widely throughout the project
	${CMAKE_CURRENT_SOURCE_DIR}/scopehal-pch.h

	${Vulkan_INCLUDE_DIR}/vulkan/vulkan_raii.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/../VkFFT/vkFFT/vkFFT.h
	)

if(${HAS_LXI})
	target_compile_definitions(scopehal PUBLIC HAS_LXI)
endif()

if(${HAS_LINUXGPIB})
	target_compile_definitions(scopehal PUBLIC HAS_LINUXGPIB)
endif()

install(TARGETS scopehal LIBRARY)
