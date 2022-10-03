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
	@brief Implementation of Filter
 */

#include "scopehal.h"
#include "Filter.h"
#ifdef __x86_64__
#include <immintrin.h>
#endif

using namespace std;

Filter::CreateMapType Filter::m_createprocs;
set<Filter*> Filter::m_filters;

mutex Filter::m_cacheMutex;
map<pair<WaveformBase*, float>, vector<int64_t> > Filter::m_zeroCrossingCache;

map<string, unsigned int> Filter::m_instanceCount;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Filter::Filter(
	const string& color,
	Category cat,
	Unit xunit)
	: OscilloscopeChannel(NULL, "", color, xunit, 0)	//TODO: handle this better?
	, m_category(cat)
	, m_usingDefault(true)
{
	m_instanceNum = 0;
	m_filters.emplace(this);

	//Create default stream gain/offset
	m_ranges.push_back(0);
	m_offsets.push_back(0);
}

Filter::~Filter()
{
	m_filters.erase(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

void Filter::ClearSweeps()
{
	//default no-op implementation
}

void Filter::AddRef()
{
	m_refcount ++;
}

void Filter::Release()
{
	m_refcount --;
	if(m_refcount == 0)
		delete this;
}

/**
	@brief Returns true if this filter outputs a waveform consisting of a single sample.

	If scalar, the output is displayed with statistics rather than a waveform view.
 */
bool Filter::IsScalarOutput()
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Enumeration

void Filter::DoAddDecoderClass(const string& name, CreateProcType proc)
{
	m_createprocs[name] = proc;
}

void Filter::EnumProtocols(vector<string>& names)
{
	for(CreateMapType::iterator it=m_createprocs.begin(); it != m_createprocs.end(); ++it)
		names.push_back(it->first);
}

Filter* Filter::CreateFilter(const string& protocol, const string& color)
{
	if(m_createprocs.find(protocol) != m_createprocs.end())
	{
		auto f = m_createprocs[protocol](color);
		f->m_instanceNum = (m_instanceCount[protocol] ++);
		return f;
	}

	LogError("Invalid filter name: %s\n", protocol.c_str());
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input verification helpers

/**
	@brief Returns true if a given input to the filter is non-NULL (and, optionally has a non-empty waveform present)
 */
bool Filter::VerifyInputOK(size_t i, bool allowEmpty)
{
	auto p = m_inputs[i];

	if(p.m_channel == NULL)
		return false;
	auto data = p.GetData();
	if(data == NULL)
		return false;

	if(!allowEmpty)
	{
		if(data->size() == 0)
			return false;
	}

	return true;
}

/**
	@brief Returns true if every input to the filter is non-NULL (and, optionally has a non-empty waveform present)
 */
bool Filter::VerifyAllInputsOK(bool allowEmpty)
{
	for(size_t i=0; i<m_inputs.size(); i++)
	{
		if(!VerifyInputOK(i, allowEmpty))
			return false;
	}

	return true;
}

/**
	@brief Returns true if every input to the filter is non-NULL and has a non-empty, uniformly sampled analog waveform present
 */
bool Filter::VerifyAllInputsOKAndUniformAnalog()
{
	for(auto p : m_inputs)
	{
		if(p.m_channel == nullptr)
			return false;

		auto data = p.m_channel->GetData(p.m_stream);
		if(data == nullptr)
			return false;
		if(data->size() == 0)
			return false;

		auto adata = dynamic_cast<UniformAnalogWaveform*>(data);
		if(adata == nullptr)
			return false;
	}

	return true;
}

/**
	@brief Returns true if every input to the filter is non-NULL and has a non-empty, sparsely sampled analog waveform present
 */
bool Filter::VerifyAllInputsOKAndSparseAnalog()
{
	for(auto p : m_inputs)
	{
		if(p.m_channel == nullptr)
			return false;

		auto data = p.m_channel->GetData(p.m_stream);
		if(data == nullptr)
			return false;
		if(data->size() == 0)
			return false;

		auto adata = dynamic_cast<SparseAnalogWaveform*>(data);
		if(adata == nullptr)
			return false;
	}

	return true;
}

/**
	@brief Returns true if every input to the filter is non-NULL and has a non-empty, sparsely sampled digital waveform present
 */
bool Filter::VerifyAllInputsOKAndSparseDigital()
{
	for(auto p : m_inputs)
	{
		if(p.m_channel == nullptr)
			return false;

		auto data = p.m_channel->GetData(p.m_stream);
		if(data == nullptr)
			return false;
		if(data->size() == 0)
			return false;

		auto ddata = dynamic_cast<SparseDigitalWaveform*>(data);
		if(ddata == nullptr)
			return false;
	}

	return true;
}

/**
	@brief Returns true if every input to the filter is non-NULL and has a non-empty, digital waveform present
 */
bool Filter::VerifyAllInputsOKAndSparseOrUniformDigital()
{
	for(auto p : m_inputs)
	{
		if(p.m_channel == nullptr)
			return false;

		auto data = p.m_channel->GetData(p.m_stream);
		if(data == nullptr)
			return false;
		if(data->size() == 0)
			return false;

		auto ddata = dynamic_cast<SparseDigitalWaveform*>(data);
		auto udata = dynamic_cast<UniformDigitalWaveform*>(data);
		if( (ddata == nullptr) && (udata == nullptr) )
			return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sampling helpers

/**
	@brief Computes durations of samples based on offsets, assuming the capture is gapless.

	The last sample has a duration of 1 unit.
 */
void Filter::FillDurationsGeneric(SparseWaveformBase& wfm)
{
	size_t len = wfm.size();
	wfm.m_durations.resize(len);
	for(size_t i=1; i<len; i++)
		wfm.m_durations[i-1] = wfm.m_offsets[i] - wfm.m_offsets[i-1];

	//Constant duration of last sample
	wfm.m_durations[len-1] = 1;
}

#ifdef __x86_64__
/**
	@brief AVX2 optimized version of FillDurationsGeneric()
 */
__attribute__((target("avx2")))
void Filter::FillDurationsAVX2(SparseWaveformBase& wfm)
{
#include <avx2intrin.h>
	size_t len = wfm.size();
	wfm.m_durations.resize(len);

	size_t end = len - (len % 4);
	int64_t* po = reinterpret_cast<int64_t*>(&wfm.m_offsets[0]);
	int64_t* pd = reinterpret_cast<int64_t*>(&wfm.m_durations[0]);
	for(size_t i=1; i<end; i+=4)
	{
		__m256i a 		= _mm256_loadu_si256(reinterpret_cast<__m256i*>(po + i));
		__m256i b 		= _mm256_loadu_si256(reinterpret_cast<__m256i*>(po + i - 1));
		__m256i delta	= _mm256_sub_epi64(a, b);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(pd + i - 1), delta);
	}

	for(size_t i=end; i<len; i++)
		wfm.m_durations[i-1] = wfm.m_offsets[i] - wfm.m_offsets[i-1];

	//Constant duration of last sample
	wfm.m_durations[len-1] = 1;
}
#endif /* __x86_64__ */

/**
	@brief Find rising edges in a waveform, interpolating to sub-sample resolution as necessary
 */
void Filter::FindRisingEdges(UniformAnalogWaveform* data, float threshold, std::vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = false;
	int64_t phoff = data->m_triggerPhase;
	size_t len = data->size();
	float fscale = data->m_timescale;

	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i] > threshold;

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}
		if(!value)
			last = false;

		//Skip samples with no rising edge
		if(!value || last)
			continue;

		//Midpoint of the sample, plus the zero crossing
		int64_t tfrac = fscale * InterpolateTime(data, i-1, threshold);
		int64_t t = phoff + data->m_timescale*(i-1) + tfrac;
		edges.push_back(t);
		last = true;
	}
}

/**
	@brief Find rising edges in a waveform, interpolating to sub-sample resolution as necessary
 */
void Filter::FindRisingEdges(SparseAnalogWaveform* data, float threshold, std::vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = false;
	int64_t phoff = data->m_triggerPhase;
	size_t len = data->size();
	float fscale = data->m_timescale;

	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i] > threshold;

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		if(!value)
			last = false;

		//Skip samples with no rising edge
		if(!value || last)
			continue;

		//Midpoint of the sample, plus the zero crossing
		int64_t tfrac = fscale * InterpolateTime(data, i-1, threshold);
		int64_t t = phoff + data->m_timescale * data->m_offsets[i-1] + tfrac;
		edges.push_back(t);
		last = true;
	}
}

/**
	@brief Find zero crossings in a waveform, interpolating as necessary
 */
void Filter::FindZeroCrossings(SparseAnalogWaveform* data, float threshold, std::vector<int64_t>& edges)
{
	pair<WaveformBase*, float> cachekey(data, threshold);

	//Check cache
	{
		lock_guard<mutex> lock(m_cacheMutex);
		auto it = m_zeroCrossingCache.find(cachekey);
		if(it != m_zeroCrossingCache.end())
		{
			edges = it->second;
			return;
		}
	}

	//Find times of the zero crossings
	bool first = true;
	bool last = false;
	int64_t phoff = data->m_triggerPhase;
	size_t len = data->m_samples.size();
	float fscale = data->m_timescale;

	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i] > threshold;

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Skip samples with no transition
		if(last == value)
			continue;

		//Midpoint of the sample, plus the zero crossing
		int64_t tfrac = fscale * InterpolateTime(data, i-1, threshold);
		int64_t t = phoff + data->m_timescale * data->m_offsets[i-1] + tfrac;
		edges.push_back(t);
		last = value;
	}

	//Add to cache
	lock_guard<mutex> lock(m_cacheMutex);
	m_zeroCrossingCache[cachekey] = edges;
}

/**
	@brief Find zero crossings in a waveform, interpolating as necessary
 */
void Filter::FindZeroCrossings(UniformAnalogWaveform* data, float threshold, std::vector<int64_t>& edges)
{
	pair<WaveformBase*, float> cachekey(data, threshold);

	//Check cache
	{
		lock_guard<mutex> lock(m_cacheMutex);
		auto it = m_zeroCrossingCache.find(cachekey);
		if(it != m_zeroCrossingCache.end())
		{
			edges = it->second;
			return;
		}
	}

	//Find times of the zero crossings
	bool first = true;
	bool last = false;
	int64_t phoff = data->m_triggerPhase;
	size_t len = data->m_samples.size();
	float fscale = data->m_timescale;

	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i] > threshold;

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Skip samples with no transition
		if(last == value)
			continue;

		//Midpoint of the sample, plus the zero crossing
		int64_t tfrac = fscale * InterpolateTime(data, i-1, threshold);
		int64_t t = phoff + data->m_timescale*(i-1) + tfrac;
		edges.push_back(t);
		last = value;
	}

	//Add to cache
	lock_guard<mutex> lock(m_cacheMutex);
	m_zeroCrossingCache[cachekey] = edges;
}

/**
	@brief Find edges in a waveform, discarding repeated samples
 */
void Filter::FindZeroCrossings(SparseDigitalWaveform* data, vector<int64_t>& edges)
{
	pair<WaveformBase*, float> cachekey(data, 0);

	//Check cache
	{
		lock_guard<mutex> lock(m_cacheMutex);
		auto it = m_zeroCrossingCache.find(cachekey);
		if(it != m_zeroCrossingCache.end())
		{
			edges = it->second;
			return;
		}
	}

	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Skip samples with no transition
		if(last == value)
			continue;

		edges.push_back(phoff + data->m_timescale * data->m_offsets[i]);
		last = value;
	}

	//Add to cache
	lock_guard<mutex> lock(m_cacheMutex);
	m_zeroCrossingCache[cachekey] = edges;
}

/**
	@brief Find edges in a waveform, discarding repeated samples
 */
void Filter::FindZeroCrossings(UniformDigitalWaveform* data, vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Skip samples with no transition
		if(last == value)
			continue;

		edges.push_back(phoff + data->m_timescale * i);
		last = value;
	}
}

/**
	@brief Find rising edges in a waveform
 */
void Filter::FindRisingEdges(SparseDigitalWaveform* data, vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Save samples with an edge
		if(value && !last)
			edges.push_back(phoff + data->m_timescale * data->m_offsets[i]);

		last = value;
	}
}

/**
	@brief Find rising edges in a waveform
 */
void Filter::FindRisingEdges(UniformDigitalWaveform* data, vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Save samples with an edge
		if(value && !last)
			edges.push_back(phoff + data->m_timescale * i);

		last = value;
	}
}

/**
	@brief Find falling edges in a waveform
 */
void Filter::FindFallingEdges(SparseDigitalWaveform* data, vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Save samples with an edge
		if(!value && last)
			edges.push_back(phoff + data->m_timescale * data->m_offsets[i]);

		last = value;
	}
}

/**
	@brief Find falling edges in a waveform
 */
void Filter::FindFallingEdges(UniformDigitalWaveform* data, vector<int64_t>& edges)
{
	//Find times of the zero crossings
	bool first = true;
	bool last = data->m_samples[0];
	int64_t phoff = data->m_timescale/2 + data->m_triggerPhase;
	size_t len = data->m_samples.size();
	for(size_t i=1; i<len; i++)
	{
		bool value = data->m_samples[i];

		//Save the last value
		if(first)
		{
			last = value;
			first = false;
			continue;
		}

		//Save samples with an edge
		if(!value && last)
			edges.push_back(phoff + data->m_timescale * i);

		last = value;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

string Filter::SerializeConfiguration(IDTable& table, size_t /*indent*/)
{
	string base = FlowGraphNode::SerializeConfiguration(table, 8);

	int id = table.emplace(this);
	string config;
	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "    filter%d:\n", id);
	config += tmp;
	snprintf(tmp, sizeof(tmp), "        id:              %d\n", id);
	config += tmp;
	config += base;

	//Channel info
	snprintf(tmp, sizeof(tmp), "        protocol:        \"%s\"\n", GetProtocolDisplayName().c_str());
	config += tmp;
	snprintf(tmp, sizeof(tmp), "        color:           \"%s\"\n", m_displaycolor.c_str());
	config += tmp;
	snprintf(tmp, sizeof(tmp), "        nick:            \"%s\"\n", m_displayname.c_str());
	config += tmp;
	snprintf(tmp, sizeof(tmp), "        name:            \"%s\"\n", GetHwname().c_str());
	config += tmp;

	//Save gain and offset (not applicable to all filters, but save it just in case)
	snprintf(tmp, sizeof(tmp), "        streams:\n");
	config += tmp;
	for(size_t i=0; i<GetStreamCount(); i++)
	{
		switch(m_streams[i].m_stype)
		{
			case Stream::STREAM_TYPE_DIGITAL:
			case Stream::STREAM_TYPE_DIGITAL_BUS:
			case Stream::STREAM_TYPE_TRIGGER:
			case Stream::STREAM_TYPE_PROTOCOL:
				break;

			default:
				snprintf(tmp, sizeof(tmp), "            stream%zu:\n", i);
				config += tmp;

				snprintf(tmp, sizeof(tmp), "                index:           %zu\n", i);
				config += tmp;

				snprintf(tmp, sizeof(tmp), "                vrange:          %f\n", GetVoltageRange(i));
				config += tmp;
				snprintf(tmp, sizeof(tmp), "                offset:          %f\n", GetOffset(i));
				config += tmp;
				break;
		}
	}

	return config;
}

void Filter::LoadParameters(const YAML::Node& node, IDTable& table)
{
	FlowGraphNode::LoadParameters(node, table);

	//id, protocol, color are already loaded
	m_displayname = node["nick"].as<string>();
	m_hwname = node["name"].as<string>();

	//Load legacy single-stream range/offset parameters
	if(node["vrange"])
		SetVoltageRange(node["vrange"].as<float>(), 0);
	if(node["offset"])
		SetOffset(node["offset"].as<float>(), 0);

	//Load stream configuration
	auto streams = node["streams"];
	if(streams)
	{
		for(auto it : streams)
		{
			auto snode = it.second;
			if(!snode["index"])
				continue;
			auto index = snode["index"].as<int>();
			if(snode["vrange"])
				SetVoltageRange(snode["vrange"].as<float>(), index);
			if(snode["offset"])
				SetOffset(snode["offset"].as<float>(), index);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interpolation helpers


/**
	@brief Interpolates the actual time of a differential threshold crossing between two samples

	Simple linear interpolation for now (TODO sinc)

	@return Interpolated crossing time. 0=a, 1=a+1, fractional values are in between.
 */
float Filter::InterpolateTime(UniformAnalogWaveform* p, UniformAnalogWaveform* n, size_t a, float voltage)
{
	//If the voltage isn't between the two points, abort
	float fa = p->m_samples[a] - n->m_samples[a];
	float fb = p->m_samples[a+1] - n->m_samples[a+1];
	bool ag = (fa > voltage);
	bool bg = (fb > voltage);
	if( (ag && bg) || (!ag && !bg) )
		return 0;

	//no need to divide by time, sample spacing is normalized to 1 timebase unit
	float slope = (fb - fa);
	float delta = voltage - fa;
	return delta / slope;
}

/**
	@brief Interpolates the actual time of a differential threshold crossing between two samples

	Simple linear interpolation for now (TODO sinc)

	@return Interpolated crossing time. 0=a, 1=a+1, fractional values are in between.
 */
float Filter::InterpolateTime(SparseAnalogWaveform* p, SparseAnalogWaveform* n, size_t a, float voltage)
{
	//If the voltage isn't between the two points, abort
	float fa = p->m_samples[a] - n->m_samples[a];
	float fb = p->m_samples[a+1] - n->m_samples[a+1];
	bool ag = (fa > voltage);
	bool bg = (fb > voltage);
	if( (ag && bg) || (!ag && !bg) )
		return 0;

	//no need to divide by time, sample spacing is normalized to 1 timebase unit
	float slope = (fb - fa);
	float delta = voltage - fa;
	return delta / slope;
}

/**
	@brief Interpolates the actual value of a point between two samples

	@param cap			Waveform to work with
	@param index		Starting position
	@param frac_ticks	Fractional position of the sample.
						Note that this is in timebase ticks, so if some samples are >1 tick apart it's possible for
						this value to be outside [0, 1].
 */
float Filter::InterpolateValue(SparseAnalogWaveform* cap, size_t index, float frac_ticks)
{
	if(index+1 > cap->size())
		return cap->m_samples[index];

	float frac = frac_ticks / (cap->m_offsets[index+1] - cap->m_offsets[index]);
	float v1 = cap->m_samples[index];
	float v2 = cap->m_samples[index+1];
	return v1 + (v2-v1)*frac;
}

/**
	@brief Interpolates the actual value of a point between two samples

	@param cap			Waveform to work with
	@param index		Starting position
	@param frac_ticks	Fractional position of the sample.
						Note that this is in timebase ticks, so if some samples are >1 tick apart it's possible for
						this value to be outside [0, 1].
 */
float Filter::InterpolateValue(UniformAnalogWaveform* cap, size_t index, float frac_ticks)
{
	if(index+1 > cap->size())
		return cap->m_samples[index];

	float v1 = cap->m_samples[index];
	float v2 = cap->m_samples[index+1];
	return v1 + (v2-v1)*frac_ticks;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Measurement helpers

void Filter::ClearAnalysisCache()
{
	lock_guard<mutex> lock(m_cacheMutex);
	m_zeroCrossingCache.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers for various common boilerplate operations

/**
	@brief Sets up an analog output waveform and copies basic metadata from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.

	@param din			Input waveform
	@param stream		Stream index
	@param clear		True to clear an existing waveform, false to leave it as-is

	@return	The ready-to-use output waveform
 */
UniformAnalogWaveform* Filter::SetupEmptyUniformAnalogOutputWaveform(WaveformBase* din, size_t stream, bool clear)
{
	//Create the waveform, but only if necessary
	auto cap = dynamic_cast<UniformAnalogWaveform*>(GetData(stream));
	if(cap == NULL)
	{
		cap = new UniformAnalogWaveform;
		SetData(cap, stream);
	}

	//Copy configuration
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;
	cap->m_timescale			= din->m_timescale;

	//Bump rev number
	cap->m_revision ++;

	//Clear output
	if(clear)
		cap->clear();

	return cap;
}

/**
	@brief Sets up an analog output waveform and copies basic metadata from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.

	@param din			Input waveform
	@param stream		Stream index
	@param clear		True to clear an existing waveform, false to leave it as-is

	@return	The ready-to-use output waveform
 */
SparseAnalogWaveform* Filter::SetupEmptySparseAnalogOutputWaveform(WaveformBase* din, size_t stream, bool clear)
{
	//Create the waveform, but only if necessary
	auto cap = dynamic_cast<SparseAnalogWaveform*>(GetData(stream));
	if(cap == NULL)
	{
		cap = new SparseAnalogWaveform;
		SetData(cap, stream);
	}

	//Copy configuration
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;
	cap->m_timescale			= din->m_timescale;

	//Bump rev number
	cap->m_revision ++;

	//Clear output
	if(clear)
		cap->clear();

	return cap;
}

/**
	@brief Sets up an digital output waveform and copies basic metadata from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.

	@param din			Input waveform
	@param stream		Stream index

	@return	The ready-to-use output waveform
 */
UniformDigitalWaveform* Filter::SetupEmptyUniformDigitalOutputWaveform(WaveformBase* din, size_t stream)
{
	//Create the waveform, but only if necessary
	auto cap = dynamic_cast<UniformDigitalWaveform*>(GetData(stream));
	if(cap == NULL)
	{
		cap = new UniformDigitalWaveform;
		SetData(cap, stream);
	}

	//Copy configuration
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;
	cap->m_timescale			= din->m_timescale;

	//Bump rev number
	cap->m_revision ++;

	//Clear output
	cap->clear();

	return cap;
}

/**
	@brief Sets up an digital output waveform and copies basic metadata from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.

	@param din			Input waveform
	@param stream		Stream index

	@return	The ready-to-use output waveform
 */
SparseDigitalWaveform* Filter::SetupEmptySparseDigitalOutputWaveform(WaveformBase* din, size_t stream)
{
	//Create the waveform, but only if necessary
	auto cap = dynamic_cast<SparseDigitalWaveform*>(GetData(stream));
	if(cap == NULL)
	{
		cap = new SparseDigitalWaveform;
		SetData(cap, stream);
	}

	//Copy configuration
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;
	cap->m_timescale			= din->m_timescale;

	//Bump rev number
	cap->m_revision ++;

	//Clear output
	cap->clear();

	return cap;
}

/**
	@brief Sets up an analog output waveform and copies timebase configuration from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.
	Timestamps are copied from the input to the output.

	@param din			Input waveform
	@param stream		Stream index
	@param skipstart	Number of input samples to discard from the beginning of the waveform
	@param skipend		Number of input samples to discard from the end of the waveform

	@return	The ready-to-use output waveform
 */
SparseAnalogWaveform* Filter::SetupSparseOutputWaveform(SparseWaveformBase* din, size_t stream, size_t skipstart, size_t skipend)
{
	auto cap = SetupEmptySparseAnalogOutputWaveform(din, stream, false);

	cap->m_timescale 			= din->m_timescale;
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;

	size_t len = din->size() - (skipstart + skipend);
	cap->Resize(len);
	cap->PrepareForCpuAccess();

	memcpy(&cap->m_offsets[0], &din->m_offsets[skipstart], len*sizeof(int64_t));
	memcpy(&cap->m_durations[0], &din->m_durations[skipstart], len*sizeof(int64_t));

	cap->MarkTimestampsModifiedFromCpu();

	return cap;
}

/**
	@brief Sets up a digital output waveform and copies timebase configuration from the input.

	A new output waveform is created if necessary, but when possible the existing one is reused.
	Timestamps are copied from the input to the output.

	@param din			Input waveform
	@param stream		Stream index
	@param skipstart	Number of input samples to discard from the beginning of the waveform
	@param skipend		Number of input samples to discard from the end of the waveform

	@return	The ready-to-use output waveform
 */
SparseDigitalWaveform* Filter::SetupSparseDigitalOutputWaveform(SparseWaveformBase* din, size_t stream, size_t skipstart, size_t skipend)
{
	//Create the waveform, but only if necessary
	auto cap = dynamic_cast<SparseDigitalWaveform*>(GetData(stream));
	if(cap == NULL)
	{
		cap = new SparseDigitalWaveform;
		SetData(cap, stream);
	}

	//Copy configuration
	cap->m_timescale 			= din->m_timescale;
	cap->m_startTimestamp 		= din->m_startTimestamp;
	cap->m_startFemtoseconds	= din->m_startFemtoseconds;
	cap->m_triggerPhase			= din->m_triggerPhase;

	size_t len = din->m_offsets.size() - (skipstart + skipend);
	cap->Resize(len);
	cap->PrepareForCpuAccess();

	memcpy(&cap->m_offsets[0], &din->m_offsets[skipstart], len*sizeof(int64_t));
	memcpy(&cap->m_durations[0], &din->m_durations[skipstart], len*sizeof(int64_t));

	cap->MarkTimestampsModifiedFromCpu();

	return cap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event driven filter processing

/**
	@brief Gets the timestamp of the next event (if any) on a waveform

	Works in timescale units
 */
int64_t Filter::GetNextEventTimestamp(SparseWaveformBase* wfm, size_t i, size_t len, int64_t timestamp)
{
	if(i+1 < len)
		return wfm->m_offsets[i+1];
	else
		return timestamp;
}

/**
	@brief Gets the timestamp of the next event (if any) on a waveform

	Works in timescale units
 */
int64_t Filter::GetNextEventTimestamp(UniformWaveformBase* /*wfm*/, size_t i, size_t len, int64_t timestamp)
{
	if(i+1 < len)
		return i+1;
	else
		return timestamp;
}

/**
	@brief Advance the waveform to a given timestamp

	Works in timescale units
 */
void Filter::AdvanceToTimestamp(SparseWaveformBase* wfm, size_t& i, size_t len, int64_t timestamp)
{
	while( ((i+1) < len) && (wfm->m_offsets[i+1] <= timestamp) )
		i ++;
}

/**
	@brief Advance the waveform to a given timestamp

	Works in timescale units
 */
void Filter::AdvanceToTimestamp(UniformWaveformBase* /*wfm*/, size_t& i, size_t /*len*/, int64_t timestamp)
{
	i = timestamp + 1;
}

/**
	@brief Gets the timestamp of the next event (if any) on a waveform

	Works in native X axis units
 */
int64_t Filter::GetNextEventTimestampScaled(SparseWaveformBase* wfm, size_t i, size_t len, int64_t timestamp)
{
	if(i+1 < len)
		return (wfm->m_offsets[i+1] * wfm->m_timescale) + wfm->m_triggerPhase;
	else
		return timestamp;
}

/**
	@brief Gets the timestamp of the next event (if any) on a waveform

	Works in native X axis units
 */
int64_t Filter::GetNextEventTimestampScaled(UniformWaveformBase* wfm, size_t i, size_t len, int64_t timestamp)
{
	if(i+1 < len)
		return ((i+1) * wfm->m_timescale) + wfm->m_triggerPhase;
	else
		return timestamp;
}

/**
	@brief Advance the waveform to a given timestamp

	Works in native X axis units
 */
void Filter::AdvanceToTimestampScaled(SparseWaveformBase* wfm, size_t& i, size_t len, int64_t timestamp)
{
	timestamp -= wfm->m_triggerPhase;

	while( ((i+1) < len) && ( (wfm->m_offsets[i+1] * wfm->m_timescale) <= timestamp) )
		i ++;
}

/**
	@brief Advance the waveform to a given timestamp

	Works in native X axis units
 */
void Filter::AdvanceToTimestampScaled(UniformWaveformBase* wfm, size_t& i, size_t len, int64_t timestamp)
{
	timestamp -= wfm->m_triggerPhase;

	while( ((i+1) < len) && ( ( static_cast<int64_t>(i+1) * wfm->m_timescale) <= timestamp) )
		i ++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Naming and other info

/**
	@brief Sets the name of a filter based on its inputs

	This method may be overridden in derived classes for specialized applications, but there is no need to do so in
	typical filters.
 */
void Filter::SetDefaultName()
{
	//Start with our immediate inputs
	set<StreamDescriptor> inputs;
	for(auto i : m_inputs)
		inputs.emplace(i);

	//If we're a measurement, stop
	//We want to see the full list of inputs as-is
	if(m_category == CAT_MEASUREMENT)
	{
	}

	//Walk filter graph back to find source nodes
	else
	{
		//Replace each input with its ancestor
		while(true)
		{
			bool changed = false;
			set<StreamDescriptor> next;

			for(auto i : inputs)
			{
				//If the channel is not a filter, it's a scope channel.
				//Pass through unchanged.
				auto f = dynamic_cast<Filter*>(i.m_channel);
				if(!f)
					next.emplace(i);

				//It's a filter. Does it have any inputs?
				//If not, it's an import or waveform generation filter. Pass through unchanged.
				else if(f->GetInputCount() == 0)
					next.emplace(i);

				//Filter that has inputs. Use them.
				else
				{
					for(size_t j=0; j<f->GetInputCount(); j++)
						next.emplace(f->GetInput(j));
					changed = true;
				}
			}

			if(!changed)
				break;
			inputs = next;
		}
	}

	//If we have any non-import inputs, hide all import inputs
	//This prevents e.g. s-parameter filenames propagating into all dependent filter names
	bool hasNonImportInputs = false;
	set<StreamDescriptor> imports;
	for(auto i : inputs)
	{
		auto f = dynamic_cast<Filter*>(i.m_channel);
		if((f != nullptr) && (f->GetInputCount() == 0) )
			imports.emplace(i);
		else
			hasNonImportInputs = true;
	}
	if(hasNonImportInputs)
	{
		for(auto i : imports)
			inputs.erase(i);
	}

	//Sort the inputs alphabetically (up to now, they're sorted by the std::set)
	vector<string> sorted;
	for(auto i : inputs)
		sorted.push_back(i.GetName());
	sort(sorted.begin(), sorted.end());

	string inames = "";
	for(auto s : sorted)
	{
		if(s == "NULL")
			continue;
		if(inames.empty())
		{
			inames = s;
			continue;
		}

		if(inames.length() + s.length() > 25)
		{
			inames += ", ...";
			break;
		}

		if(inames != "")
			inames += ",";
		inames += s;
	}

	//Format final output: remove spaces from display name, add instance number
	auto pname = GetProtocolDisplayName();
	string pname2;
	for(auto c : pname)
	{
		if(isalnum(c))
			pname2 += c;
	}
	string name = pname2 + +"_" + to_string(m_instanceNum + 1);
	if(!inames.empty())
		name += "(" + inames + ")";

	m_hwname = name;
	m_displayname = name;

}

/**
	@brief Determines if we need to display the configuration / setup dialog

	The default implementation returns true if we have more than one input or any parameters, and false otherwise.
 */
bool Filter::NeedsConfig()
{
	if(m_parameters.size())
		return true;
	if(m_inputs.size() > 1)
		return true;
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertical scaling

void Filter::ClearStreams()
{
	OscilloscopeChannel::ClearStreams();
	m_ranges.clear();
	m_offsets.clear();
}

void Filter::AddStream(Unit yunit, const string& name, Stream::StreamType stype, uint8_t flags)
{
	OscilloscopeChannel::AddStream(yunit, name, stype, flags);
	m_ranges.push_back(0);
	m_offsets.push_back(0);
}

/**
	@brief Adjusts gain and offset such that the active waveform occupies the entire vertical area of the plot
 */
void Filter::AutoscaleVertical(size_t stream)
{
	auto data = GetData(stream);
	auto swfm = dynamic_cast<SparseAnalogWaveform*>(data);
	auto uwfm = dynamic_cast<UniformAnalogWaveform*>(data);
	if(!swfm && !uwfm)
	{
		LogTrace("No waveform\n");
		return;
	}
	data->PrepareForCpuAccess();

	float vmin = GetMinVoltage(swfm, uwfm);
	float vmax = GetMaxVoltage(swfm, uwfm);

	float range = vmax - vmin;
	if(IsScalarOutput())
		range = vmax * 0.05;

	SetVoltageRange(range * 1.05, stream);
	SetOffset(-(vmin + vmax) / 2, stream);
}

float Filter::GetVoltageRange(size_t stream)
{
	if(m_ranges[stream] == 0)
	{
		if(GetData(stream) == nullptr)
			return 1;

		AutoscaleVertical(stream);
	}

	return m_ranges[stream];
}

void Filter::SetVoltageRange(float range, size_t stream)
{
	m_ranges[stream] = range;
}

float Filter::GetOffset(size_t stream)
{
	if(m_ranges[stream] == 0)
	{
		if(GetData(stream) == nullptr)
			return 0;

		AutoscaleVertical(stream);
	}

	return m_offsets[stream];
}

void Filter::SetOffset(float offset, size_t stream)
{
	m_offsets[stream] = offset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accelerated waveform processing

/**
	@brief Gets the desired location of the filter's output data

	The default implementation returns CPU.

	@return		LOC_CPU: if the filter assumes input waveforms are readable from the CPU
				LOC_GPU: if the filter assumes input waveforms are readable from the GPU
				LOC_DONTCARE: if the filter manages its own input memory, or can work with either CPU or GPU input
 */
Filter::DataLocation Filter::GetInputLocation()
{
	return LOC_CPU;
}

/**
	@brief Evaluates the filter.

	This version does not support using Vulkan acceleration and should be considered deprecated. It will be
	removed in the indefinite future once all filters have been converted to the new API.
 */
void Filter::Refresh()
{
}

/**
	@brief Evaluates the filter, using GPU acceleration if possible

	The default implementation calls the legacy non-accelerated Refresh() method.
 */
void Filter::Refresh(vk::raii::CommandBuffer& /*cmdBuf*/, vk::raii::Queue& /*queue*/)
{
	Refresh();

	//Mark our outputs as modified CPU side
	for(size_t i=0; i<m_streams.size(); i++)
	{
		auto data = m_streams[i].m_waveform;
		if(data)
			data->MarkSamplesModifiedFromCpu();
	}
}
