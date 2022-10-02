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

#include "ImportFilter.h"
#include "ScopehalUtils.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ImportFilter::ImportFilter(const ScopehalColor color, Unit xunit)
	: Filter(color, CAT_GENERATION, xunit)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool ImportFilter::ValidateChannel(size_t /*i*/, StreamDescriptor /*stream*/)
{
	//no inputs
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

void ImportFilter::SetDefaultName()
{
	auto fname = m_parameters[m_fpname].ToString();

	char hwname[256];
	snprintf(hwname, sizeof(hwname), "%s", BaseName(fname).c_str());
	m_hwname = hwname;
	m_displayname = m_hwname;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

bool ImportFilter::NeedsConfig()
{
	return true;
}

void ImportFilter::Refresh()
{
	//everything happens in OnFileNameChanged
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Import helpers

/**
	@brief Cleans up timebase of data that might be regularly or irregularly sampled.

	This function identifies data sampled at regular intervals and adjusts the timescale and sample duration/offset
	values accordingly, to enable dense packed optimizations and proper display of instrument timebase settings on
	imported waveforms.

	This function doesn't actually generate a uniform waveform, the caller has to take care of that.
 */
bool ImportFilter::TryNormalizeTimebase(SparseWaveformBase* wfm)
{
	//Find the mean sample interval
	Unit fs(Unit::UNIT_FS);
	uint64_t interval_sum = 0;
	uint64_t interval_count = wfm->size();
	for(size_t i=0; i<interval_count; i++)
		interval_sum += wfm->m_durations[i];
	uint64_t avg = interval_sum / interval_count;
	LogTrace("Average sample interval: %s\n", fs.PrettyPrint(avg).c_str());

	//Find the standard deviation of sample intervals
	uint64_t stdev_sum = 0;
	for(size_t i=0; i<interval_count; i++)
	{
		int64_t delta = (wfm->m_durations[i] - avg);
		stdev_sum += delta*delta;
	}
	uint64_t stdev = sqrt(stdev_sum / interval_count);
	LogTrace("Stdev of intervals: %s\n", fs.PrettyPrint(stdev).c_str());

	//If the standard deviation is more than 2% of the average sample period, assume the data is sampled irregularly.
	if( (stdev * 50) > avg)
	{
		LogTrace("Deviation is too large, assuming non-uniform sample interval\n");
		return false;
	}

	//If we get here, assume uniform sampling.
	//Use time zero as the trigger phase.
	wfm->m_timescale = avg;
	wfm->m_triggerPhase = wfm->m_offsets[0];
	size_t len = wfm->m_offsets.size();
	for(size_t j=0; j<len; j++)
	{
		wfm->m_offsets[j] = j;
		wfm->m_durations[j] = 1;
	}
	return true;
}
