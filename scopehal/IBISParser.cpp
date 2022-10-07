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
	@brief Implementation of IBISParser and related classes
 */
#include "IBISParser.h"
#include "ScopehalUtils.h"
#include "log.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IVCurve

float IVCurve::InterpolateCurrent(float voltage)
{
	//Binary search to find the points straddling us
	size_t len = m_curve.size();
	size_t pos = len/2;
	size_t last_lo = 0;
	size_t last_hi = len - 1;

	if(len == 0)
		return 0;

	//If out of range, clip
	if(voltage < m_curve[0].m_voltage)
		return m_curve[0].m_current;
	else if(voltage > m_curve[len-1].m_voltage)
		return m_curve[len-1].m_current;
	else
	{
		while(true)
		{
			//Dead on? Stop
			if( (last_hi - last_lo) <= 1)
				break;

			//Too high, move down
			if(m_curve[pos].m_voltage > voltage)
			{
				size_t delta = (pos - last_lo);
				last_hi = pos;
				pos = last_lo + delta/2;
			}

			//Too low, move up
			else
			{
				size_t delta = last_hi - pos;
				last_lo = pos;
				pos = last_hi - delta/2;
			}
		}
	}

	//Find position between the points for interpolation
	float vlo = m_curve[last_lo].m_voltage;
	float vhi = m_curve[last_hi].m_voltage;
	float dv = vhi - vlo;
	float frac = (voltage - vlo) / dv;

	//Interpolate current
	float ilo = m_curve[last_lo].m_current;
	float ihi = m_curve[last_hi].m_current;
	return ilo + (ihi - ilo)*frac;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VTCurves

float VTCurves::InterpolateVoltage(IBISCorner corner, float time)
{
	//Binary search to find the points straddling us
	size_t len = m_curves[corner].size();
	size_t pos = len/2;
	size_t last_lo = 0;
	size_t last_hi = len - 1;

	if(len == 0)
		return 0;

	//If out of range, clip
	if(time < m_curves[corner][0].m_time)
		return m_curves[corner][0].m_voltage;
	else if(time > m_curves[corner][len-1].m_time)
		return m_curves[corner][len-1].m_voltage;
	else
	{
		while(true)
		{
			//Dead on? Stop
			if( (last_hi - last_lo) <= 1)
				break;

			//Too high, move down
			if(m_curves[corner][pos].m_time > time)
			{
				size_t delta = (pos - last_lo);
				last_hi = pos;
				pos = last_lo + delta/2;
			}

			//Too low, move up
			else
			{
				size_t delta = last_hi - pos;
				last_lo = pos;
				pos = last_hi - delta/2;
			}
		}
	}

	//Find position between the points for interpolation
	float tlo = m_curves[corner][last_lo].m_time;
	float thi = m_curves[corner][last_hi].m_time;
	float dt = thi - tlo;
	float frac = (time - tlo) / dt;

	//Interpolate voltage
	float vlo = m_curves[corner][last_lo].m_voltage;
	return vlo + (m_curves[corner][last_hi].m_voltage - vlo)*frac;
}

/**
	@brief Gets the propagation delay of a V/T curve

	The propagation delay is defined as the timestamp at which the output voltage changes by more than 0.1% from
	the initial value.
 */
int64_t VTCurves::GetPropagationDelay(IBISCorner corner)
{
	auto& curve = m_curves[corner];

	float threshold = curve[0].m_voltage * 0.001;
	for(auto& p : curve)
	{
		if(fabs(p.m_voltage - curve[0].m_voltage) > threshold)
			return p.m_time * FS_PER_SECOND;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IBISModel

/**
	@brief Get the falling-edge waveform terminated to ground (or lowest available voltage)
 */
VTCurves* IBISModel::GetLowestFallingWaveform()
{
	VTCurves* ret = &m_falling[0];
	for(auto& curve : m_falling)
	{
		if(curve.m_fixtureVoltage < ret->m_fixtureVoltage)
			ret = &curve;
	}
	return ret;
}

/**
	@brief Get the rising-edge waveform terminated to ground (or lowest available voltage)
 */
VTCurves* IBISModel::GetLowestRisingWaveform()
{
	VTCurves* ret = &m_rising[0];
	for(auto& curve : m_rising)
	{
		if(curve.m_fixtureVoltage < ret->m_fixtureVoltage)
			ret = &curve;
	}
	return ret;
}

/**
	@brief Get the falling-edge waveform terminated to Vcc (or highest available voltage)
 */
VTCurves* IBISModel::GetHighestFallingWaveform()
{
	VTCurves* ret = &m_falling[0];
	for(auto& curve : m_falling)
	{
		if(curve.m_fixtureVoltage > ret->m_fixtureVoltage)
			ret = &curve;
	}
	return ret;
}

/**
	@brief Get the rising-edge waveform terminated to Vcc (or lowest available voltage)
 */
VTCurves* IBISModel::GetHighestRisingWaveform()
{
	VTCurves* ret = &m_rising[0];
	for(auto& curve : m_rising)
	{
		if(curve.m_fixtureVoltage > ret->m_fixtureVoltage)
			ret = &curve;
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IBISParser

IBISParser::IBISParser()
{
}

IBISParser::~IBISParser()
{
	Clear();
}

void IBISParser::Clear()
{
	for(auto it : m_models)
		delete it.second;
	m_models.clear();
}

bool IBISParser::Load(string fname)
{
	FILE* fp = fopen(fname.c_str(), "r");
	if(!fp)
	{
		LogError("IBIS file \"%s\" could not be opened\n", fname.c_str());
		return false;
	}

	//Comment char defaults to pipe, but can be changed (weird)
	char comment = '|';

	enum
	{
		BLOCK_NONE,
		BLOCK_PULLDOWN,
		BLOCK_PULLUP,
		BLOCK_GND_CLAMP,
		BLOCK_POWER_CLAMP,
		BLOCK_RISING_WAVEFORM,
		BLOCK_FALLING_WAVEFORM,
		BLOCK_MODEL_SPEC,
		BLOCK_RAMP,
		BLOCK_SUBMODEL,
		BLOCK_MODEL_SELECTOR
	} data_block = BLOCK_NONE;

	//IBIS file is line oriented, so fetch an entire line then figure out what to do with it.
	//Per IBIS 6.0 spec rule 3.4, files cannot be >120 chars so if we truncate at 127 we should be good.
	char line[128];
	char command[128];
	char tmp[128];
	IBISModel* model = NULL;
	VTCurves waveform;
	while(!feof(fp))
	{
		if(fgets(line, sizeof(line), fp) == NULL)
			break;

		//Skip comments
		if(line[0] == comment)
			continue;

		//Parse commands
		if(line[0] == '[')
		{
			if(1 != sscanf(line, "[%127[^]]]", command))
				continue;
			string scmd(command);

			//If in a waveform, save it when the block ends
			if(data_block == BLOCK_RISING_WAVEFORM)
				model->m_rising.push_back(waveform);
			else if(data_block == BLOCK_FALLING_WAVEFORM)
				model->m_falling.push_back(waveform);

			//End of file
			if(scmd == "END")
				break;

			//Metadata
			if(scmd == "Component")
			{
				sscanf(line, "[Component] %127s", tmp);
				m_component = tmp;
			}
			else if(scmd == "Manufacturer")
			{
				sscanf(line, "[Manufacturer] %127s", tmp);
				m_manufacturer = tmp;
			}
			else if( (scmd == "IBIS ver") || (scmd == "IBIS Ver") )
			{}
			else if(scmd == "File name")
			{}
			else if(scmd == "File Rev")
			{}
			else if(scmd == "Date")
			{}
			else if(scmd == "Source")
			{}
			else if(scmd == "Notes")
			{}
			else if(scmd == "Disclaimer")
			{}
			else if(scmd == "Copyright")
			{}
			else if(scmd == "Package")
			{}

			//Start a new model
			else if(scmd == "Model")
			{
				sscanf(line, "[Model] %127s", tmp);
				model = new IBISModel(tmp);
				m_models[tmp] = model;
				data_block = BLOCK_NONE;
			}

			//Start a new section
			else if(scmd == "Pullup")
				data_block = BLOCK_PULLUP;
			else if(scmd == "Pulldown")
				data_block = BLOCK_PULLDOWN;
			else if( (scmd == "GND_clamp") || (scmd == "GND Clamp") )
				data_block = BLOCK_GND_CLAMP;
			else if( (scmd == "POWER_clamp") || (scmd == "POWER Clamp") )
				data_block = BLOCK_POWER_CLAMP;
			else if(scmd == "Rising Waveform")
			{
				data_block = BLOCK_RISING_WAVEFORM;
				for(int i=0; i<3; i++)
					waveform.m_curves[i].clear();
			}
			else if(scmd == "Falling Waveform")
			{
				data_block = BLOCK_FALLING_WAVEFORM;
				for(int i=0; i<3; i++)
					waveform.m_curves[i].clear();
			}
			else if( (scmd == "Model Spec") || (scmd == "Model spec") )
				data_block = BLOCK_MODEL_SPEC;
			else if(scmd == "Ramp")
				data_block = BLOCK_RAMP;
			else if(scmd == "Add Submodel")
				data_block = BLOCK_SUBMODEL;
			else if(scmd == "Model Selector")
				data_block = BLOCK_MODEL_SELECTOR;

			//TODO: Terminations
			else if(scmd == "R Series")
			{}

			//Ignore pin table
			else if( (scmd == "Pin") || (scmd == "Diff Pin") | (scmd == "Series Pin Mapping"))
			{
				data_block = BLOCK_NONE;
				model = NULL;
			}

			//TODO: submodels
			else if(scmd == "Submodel")
			{
				data_block = BLOCK_NONE;
				model = NULL;
			}

			//One-line specifications
			else if(scmd == "Temperature Range")
			{
				sscanf(
					line,
					"[Temperature Range] %f %f %f",
					&model->m_temps[CORNER_TYP],
					&model->m_temps[CORNER_MIN],
					&model->m_temps[CORNER_MAX]);
			}
			else if(scmd == "Voltage Range")
			{
				sscanf(
					line,
					"[Voltage Range] %f %f %f",
					&model->m_voltages[CORNER_TYP],
					&model->m_voltages[CORNER_MIN],
					&model->m_voltages[CORNER_MAX]);
			}
			else if(scmd == "Power Clamp Reference")
			{
				//ignore for now
			}
			else if(scmd == "GND Clamp Reference")
			{
				//ignore for now
			}
			else if(scmd == "Pullup Reference")
			{
				//ignore for now
			}
			else if(scmd == "Pulldown Reference")
			{
				//ignore for now
			}

			//TODO: IBIS 5.0 SSO
			//See https://www.micron.com/-/media/client/global/documents/products/technical-note/dram/tn_0033_ibis_models_power_integrity_simulation.pdf
			else if( (scmd == "ISSO PU") || (scmd == "ISSO PD") )
			{
				data_block = BLOCK_NONE;
			}

			//TODO: IBIS 5.0 composite current
			else if(scmd == "Composite Current")
			{
				data_block = BLOCK_NONE;
			}

			//TODO: not sure what this is yet
			else if(scmd == "Driver Schedule")
			{
				data_block = BLOCK_NONE;
			}

			else
			{
				LogWarning("Unrecognized command %s\n", command);
			}

			continue;
		}

		//Alphanumeric? It's a keyword. Parse it out.
		else if(isalpha(line[0]))
		{
			sscanf(line, "%127[^ =]", tmp);
			string skeyword = tmp;

			//If there's not an active model, skip it
			if(!model)
				continue;

			//Skip anything in a submodel section
			if(data_block == BLOCK_SUBMODEL)
				continue;

			//Type of buffer
			if(skeyword == "Model_type")
			{
				if(1 != sscanf(line, "Model_type %127s", tmp))
					continue;

				string type(tmp);
				if(type == "I/O")
					model->m_type = IBISModel::TYPE_IO;
				else if(type == "Input")
					model->m_type = IBISModel::TYPE_INPUT;
				else if(type == "Output")
					model->m_type = IBISModel::TYPE_OUTPUT;
				else if(type == "Open_drain")
					model->m_type = IBISModel::TYPE_OPEN_DRAIN;
				else if(type == "Series")
					model->m_type = IBISModel::TYPE_SERIES;
				else if(type == "Terminator")
					model->m_type = IBISModel::TYPE_TERMINATOR;
				else
					LogWarning("Don't know what to do with Model_type %s\n", tmp);
			}

			//Input thresholds
			//The same keywords appear under the [Model] section. Ignore these and only grab the full corners
			else if(skeyword == "Vinl")
			{
				if(data_block == BLOCK_MODEL_SPEC)
				{
					sscanf(
						line,
						"Vinl %f %f %f",
						&model->m_vil[CORNER_TYP],
						&model->m_vil[CORNER_MIN],
						&model->m_vil[CORNER_MAX]);
				}
			}
			else if(skeyword == "Vinh")
			{
				if(data_block == BLOCK_MODEL_SPEC)
				{
					sscanf(
						line,
						"Vinh %f %f %f",
						&model->m_vih[CORNER_TYP],
						&model->m_vih[CORNER_MIN],
						&model->m_vih[CORNER_MAX]);
				}
			}

			//Ignore various metadata about the buffer
			else if(skeyword == "Polarity")
			{}
			else if(skeyword == "Enable")
			{}
			else if(skeyword == "Vmeas")
			{}
			else if(skeyword == "Cref")
			{}
			else if(skeyword == "Rref")
			{}
			else if(skeyword == "Vref")
			{}

			//Die capacitance
			else if(skeyword == "C_comp")
			{
				char scale[3];
				sscanf(
					line,
					"C_comp %f%cF %f%cF %f%cF",
					&model->m_dieCapacitance[CORNER_TYP],
					&scale[CORNER_TYP],
					&model->m_dieCapacitance[CORNER_MIN],
					&scale[CORNER_MIN],
					&model->m_dieCapacitance[CORNER_MAX],
					&scale[CORNER_MAX]);

				for(int i=0; i<3; i++)
				{
					if(scale[i] == 'p')
						model->m_dieCapacitance[i] *= 1e-12;
					else if(scale[i] == 'n')
						model->m_dieCapacitance[i] *= 1e-9;
					else if(scale[i] == 'u')
						model->m_dieCapacitance[i] *= 1e-6;
				}
			}

			//Fixture properties in waveforms
			else if(skeyword == "R_fixture")
			{
				char fres[128];
				sscanf(line, "R_fixture = %127s", fres);
				waveform.m_fixtureResistance = ParseNumber(fres);
			}

			else if(skeyword == "V_fixture")
				sscanf(line, "V_fixture = %f", &waveform.m_fixtureVoltage);

			else if(skeyword == "V_fixture_min")
			{}
			else if(skeyword == "V_fixture_max")
			{}
			else if(skeyword == "R_load")
			{}

			//Ramp rate
			else if(skeyword == "dV/dt_r")
			{}
			else if(skeyword == "dV/dt_f")
			{}

			//Something else we havent seen before
			else
			{
				LogWarning("Unrecognized keyword %s\n", tmp);
			}
		}

		//If we get here, it's a data table.
		else
		{
			//If not in a data block, do nothing
			if(data_block == BLOCK_NONE)
				continue;

			//If there's not an active model, skip it
			if(!model)
				continue;

			//Crack individual numbers
			char sindex[32] = {0};
			char styp[32];
			char smin[32];
			char smax[32];
			if(4 != sscanf(line, " %31[^ ] %31[^ ] %31[^ ] %31[^ \n]", sindex, styp, smin, smax))
				continue;

			//Parse the numbers
			float index = ParseNumber(sindex);
			float vtyp = ParseNumber(styp);
			float vmin = ParseNumber(smin);
			float vmax = ParseNumber(smax);

			switch(data_block)
			{
				//Curves
				case BLOCK_PULLDOWN:
					model->m_pulldown[CORNER_TYP].m_curve.push_back(IVPoint(index, vtyp));
					model->m_pulldown[CORNER_MIN].m_curve.push_back(IVPoint(index, vmin));
					model->m_pulldown[CORNER_MAX].m_curve.push_back(IVPoint(index, vmax));
					break;

				case BLOCK_PULLUP:
					model->m_pullup[CORNER_TYP].m_curve.push_back(IVPoint(index, vtyp));
					model->m_pullup[CORNER_MIN].m_curve.push_back(IVPoint(index, vmin));
					model->m_pullup[CORNER_MAX].m_curve.push_back(IVPoint(index, vmax));
					break;

				case BLOCK_RISING_WAVEFORM:
				case BLOCK_FALLING_WAVEFORM:
					waveform.m_curves[CORNER_TYP].push_back(VTPoint(index, vtyp));
					waveform.m_curves[CORNER_MIN].push_back(VTPoint(index, vmin));
					waveform.m_curves[CORNER_MAX].push_back(VTPoint(index, vmax));
					break;

				//Ignore other curves for now
				default:
					break;
			}
		}
	}

	fclose(fp);
	return true;
}

float IBISParser::ParseNumber(const char* str)
{
	//Pull out the digits
	string digits;
	char scale = ' ';
	bool foundE = false;
	for(size_t i=0; i<32; i++)
	{
		char c = str[i];

		if( (c == '-') || (c == '.') || isdigit(c))
			digits += c;

		else if(c == 'e')
		{
			digits += c;
			foundE = true;
		}

		else if(isspace(c))
			continue;

		else if(c == '\0')
			break;

		else
		{
			scale = c;
			break;
		}
	}

	float ret;
	if(foundE)
		sscanf(digits.c_str(), "%e", &ret);
	else
		sscanf(digits.c_str(), "%f", &ret);

	switch(scale)
	{
		case 'k':
			return ret * 1e3;

		case 'm':
			return ret * 1e-3;

		case 'u':
			return ret * 1e-6;

		case 'n':
			return ret * 1e-9;

		case 'p':
			return ret * 1e-12;

		default:
			return ret;
	}
}
