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

#include "ScaleFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ScaleFilter::ScaleFilter(const ScopehalColor color)
	: Filter(color, CAT_MATH)
{
	AddStream(Unit(Unit::UNIT_VOLTS), "data", Stream::STREAM_TYPE_ANALOG);
	CreateInput("din");

	m_scalefactorname = "Scale Factor";
	m_parameters[m_scalefactorname] = FilterParameter(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_scalefactorname].SetFloatVal(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool ScaleFilter::ValidateChannel(size_t i, StreamDescriptor stream)
{
	if(stream.m_channel == NULL)
		return false;

	if( (i == 0) && (stream.GetType() == Stream::STREAM_TYPE_ANALOG) )
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string ScaleFilter::GetProtocolName()
{
	return "Scale";
}

void ScaleFilter::SetDefaultName()
{
	char hwname[256];
	float scalefactor = m_parameters[m_scalefactorname].GetFloatVal();
	snprintf(hwname, sizeof(hwname), "%s * %.3f", GetInputDisplayName(0).c_str(), scalefactor);
	m_hwname = hwname;
	m_displayname = m_hwname;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void ScaleFilter::Refresh()
{
	//Make sure we've got valid inputs
	if(!VerifyAllInputsOK())
	{
		SetData(NULL, 0);
		return;
	}

	auto din = GetInputWaveform(0);
	din->PrepareForCpuAccess();
	size_t len = din->size();
	float scalefactor = m_parameters[m_scalefactorname].GetFloatVal();

	auto udin = dynamic_cast<UniformAnalogWaveform*>(din);
	auto sdin = dynamic_cast<SparseAnalogWaveform*>(din);

	//Multiply all of our samples by the scale factor
	if(udin)
	{
		auto cap = SetupEmptyUniformAnalogOutputWaveform(din, 0);
		cap->Resize(len);
		cap->PrepareForCpuAccess();

		float* out = (float*)__builtin_assume_aligned(cap->m_samples.GetCpuPointer(), 16);
		float* a = (float*)__builtin_assume_aligned(udin->m_samples.GetCpuPointer(), 16);

		for(size_t i=0; i<len; i++)
			out[i] = a[i] * scalefactor;

		cap->MarkModifiedFromCpu();
	}
	else
	{
		auto cap = SetupSparseOutputWaveform(sdin, 0, 0, 0);
		cap->PrepareForCpuAccess();

		float* out = (float*)__builtin_assume_aligned(cap->m_samples.GetCpuPointer(), 16);
		float* a = (float*)__builtin_assume_aligned(sdin->m_samples.GetCpuPointer(), 16);

		for(size_t i=0; i<len; i++)
			out[i] = a[i] * scalefactor;

		cap->MarkModifiedFromCpu();
	}
}
