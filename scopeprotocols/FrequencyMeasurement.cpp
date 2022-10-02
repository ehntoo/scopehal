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

#include "FrequencyMeasurement.h"
#include "ScopehalUtils.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FrequencyMeasurement::FrequencyMeasurement(const ScopehalColor color)
	: Filter(color, CAT_MEASUREMENT)
{
	AddStream(Unit(Unit::UNIT_HZ), "data", Stream::STREAM_TYPE_ANALOG);

	//Set up channels
	CreateInput("din");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool FrequencyMeasurement::ValidateChannel(size_t i, StreamDescriptor stream)
{
	if(stream.m_channel == NULL)
		return false;

	if(i > 0)
		return false;

	if( (stream.GetType() == Stream::STREAM_TYPE_ANALOG) ||
		(stream.GetType() == Stream::STREAM_TYPE_DIGITAL) )
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string FrequencyMeasurement::GetProtocolName()
{
	return "Frequency";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void FrequencyMeasurement::Refresh()
{
	//Make sure we've got valid inputs
	if(!VerifyAllInputsOK())
	{
		SetData(NULL, 0);
		return;
	}

	auto din = GetInputWaveform(0);
	din->PrepareForCpuAccess();
	auto uadin = dynamic_cast<UniformAnalogWaveform*>(din);
	auto sadin = dynamic_cast<SparseAnalogWaveform*>(din);
	auto uddin = dynamic_cast<UniformDigitalWaveform*>(din);
	auto sddin = dynamic_cast<SparseDigitalWaveform*>(din);
	vector<int64_t> edges;

	//Auto-threshold analog signals at 50% of full scale range
	if(uadin)
		FindZeroCrossings(uadin, GetAvgVoltage(uadin), edges);
	else if(sadin)
		FindZeroCrossings(sadin, GetAvgVoltage(sadin), edges);

	//Just find edges in digital signals
	else if(uddin)
		FindZeroCrossings(uddin, edges);
	else
		FindZeroCrossings(sddin, edges);

	//We need at least one full cycle of the waveform to have a meaningful frequency
	if(edges.size() < 2)
	{
		SetData(NULL, 0);
		return;
	}

	//Create the output
	auto cap = SetupEmptySparseAnalogOutputWaveform(din, 0, true);
	cap->m_timescale = 1;
	cap->PrepareForCpuAccess();

	size_t elen = edges.size();
	for(size_t i=0; i < (elen - 2); i+= 2)
	{
		//measure from edge to 2 edges later, since we find all zero crossings regardless of polarity
		int64_t start = edges[i];
		int64_t end = edges[i+2];

		int64_t delta = end - start;
		double freq = FS_PER_SECOND / delta;

		cap->m_offsets.push_back(start);
		cap->m_durations.push_back(round(delta));
		cap->m_samples.push_back(freq);
	}

	SetData(cap, 0);

	cap->MarkModifiedFromCpu();
}
