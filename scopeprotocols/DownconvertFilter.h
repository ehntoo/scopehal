/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
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
	@brief Declaration of DownconvertFilter
 */
#ifndef DownconvertFilter_h
#define DownconvertFilter_h

/**
	@brief Downconvert - generates a local oscillator in two phases and mixes it with a signal
 */
class DownconvertFilter : public Filter
{
public:
	DownconvertFilter(const std::string& color);

	virtual void Refresh();

	static std::string GetProtocolName();

	virtual bool ValidateChannel(size_t i, StreamDescriptor stream);

	PROTOCOL_DECODER_INITPROC(DownconvertFilter)

protected:
	std::string m_freqname;

	void DoFilterKernelGeneric(
		UniformAnalogWaveform* din,
		UniformAnalogWaveform* cap_i,
		UniformAnalogWaveform* cap_q,
		float lo_rad_per_sample,
		float trigger_phase_rad);

#ifdef __x86_64__
	__attribute__((target("avx2")))
	void DoFilterKernelAVX2DensePacked(
		UniformAnalogWaveform* din,
		UniformAnalogWaveform* cap_i,
		UniformAnalogWaveform* cap_q,
		float lo_rad_per_sample,
		float trigger_phase_rad);
	__attribute__((target("default")))
	void DoFilterKernelAVX2DensePacked(
		UniformAnalogWaveform* din,
		UniformAnalogWaveform* cap_i,
		UniformAnalogWaveform* cap_q,
		float lo_rad_per_sample,
		float trigger_phase_rad);
#endif
};

#endif
