/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg and contributors                                                         *
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

#ifndef RFSignalGenerator_h
#define RFSignalGenerator_h

#include "Instrument.h"

/**
	@brief An RF waveform generator which creates a carrier and optionally modulates it
 */
class RFSignalGenerator : public virtual Instrument
{
public:
	RFSignalGenerator();
	virtual ~RFSignalGenerator();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// General

	/**
		@brief Returns the number of output channels on the generator
	 */
	virtual int GetChannelCount() =0;

	/**
		@brief Returns the name of a given output channel

		@param chan	Zero-based channel index
	 */
	virtual std::string GetChannelName(int chan) =0;

	/**
		@brief Check if a channel is currently enabled

		@param chan	Zero-based channel index

		@return True if output is enabled, false if disabled
	 */
	virtual bool GetChannelOutputEnable(int chan) =0;

	/**
		@brief Enable or disable a channel output

		@param chan	Zero-based channel index
		@param on	True to enable the output, false to disable
	 */
	virtual void SetChannelOutputEnable(int chan, bool on) =0;

	/**
		@brief Gets the power level of a channel

		@param chan	Zero-based channel index

		@return Power level (in dBm)
	 */
	virtual float GetChannelOutputPower(int chan) =0;

	/**
		@brief Sets the power level of a channel

		@param chan		Zero-based channel index
		@param power	Power level (in dBm)
	 */
	virtual void SetChannelOutputPower(int chan, float power) =0;

	/**
		@brief Gets the center frequency of a channel

		@param chan	Zero-based channel index

		@return Center frequency, in Hz
	 */
	virtual float GetChannelCenterFrequency(int chan) =0;

	/**
		@brief Sets the power level of a channel

		@param chan		Zero-based channel index
		@param freq		Center frequency, in Hz
	 */
	virtual void SetChannelCenterFrequency(int chan, float freq) =0;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Vector modulation

	/**
		@brief Checks if an instrument is vector modulation capable

		@param chan		Zero-based channel index
	 */
	virtual bool IsVectorModulationAvailable(int chan) =0;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sweeps

	/**
		@brief Checks if an instrument supports sweeping the center frequency
	 */
	virtual bool IsSweepAvailable(int chan) =0;

	enum SweepType
	{
		SWEEP_TYPE_NONE,
		SWEEP_TYPE_FREQ,
		SWEEP_TYPE_LEVEL,
		SWEEP_TYPE_FREQ_LEVEL
	};

	/**
		@brief Gets the type of a sweep

		@param chan		Zero-based channel index
	 */
	virtual SweepType GetSweepType(int chan);

	/**
		@brief Sets the type of a sweep

		@param chan		Zero-based channel index
		@param type		Sweep type
	 */
	virtual void SetSweepType(int chan, SweepType type);

	/**
		@brief Gets the start of a frequency sweep, in Hz

		@param chan		Zero-based channel index
	 */
	virtual float GetSweepStartFrequency(int chan);

	/**
		@brief Gets the end of a frequency sweep, in Hz

		@param chan		Zero-based channel index
	 */
	virtual float GetSweepStopFrequency(int chan);

	/**
		@brief Sets the start of a frequency sweep

		@param chan		Zero-based channel index
		@param freq		Start frequency, in Hz
	 */
	virtual void SetSweepStartFrequency(int chan, float freq);

	/**
		@brief Sets the stop of a frequency sweep

		@param chan		Zero-based channel index
		@param freq		Stop frequency, in Hz
	 */
	virtual void SetSweepStopFrequency(int chan, float freq);

	/**
		@brief Gets the start of a power sweep, in dBm

		@param chan		Zero-based channel index
	 */
	virtual float GetSweepStartLevel(int chan);

	/**
		@brief Gets the end of a power sweep, in dBm

		@param chan		Zero-based channel index
	 */
	virtual float GetSweepStopLevel(int chan);

	/**
		@brief Sets the start of a power sweep

		@param chan		Zero-based channel index
		@param level	Start power, in dBm
	 */
	virtual void SetSweepStartLevel(int chan, float level);

	/**
		@brief Sets the stop of a power sweep

		@param chan		Zero-based channel index
		@param level	Stop power, in dBm
	 */
	virtual void SetSweepStopLevel(int chan, float level);

	/**
		@brief Sets the dwell time for each step in a sweep

		@param chan		Zero-based channel index
		@param step		Step, in femtoseconds
	 */
	virtual void SetSweepDwellTime(int chan, float fs);

	/**
		@brief Gets the dwell time for each step in a sweep, in femtoseconds

		@param chan		Zero-based channel index
	 */
	virtual float GetSweepDwellTime(int chan);

	/**
		@brief Sets the number of frequency points in a sweep

		@param chan		Zero-based channel index
		@param npoints	Number of points
	 */
	virtual void SetSweepPoints(int chan, int npoints);

	/**
		@brief Gets the number of frequency points in a sweep

		@param chan		Zero-based channel index
	 */
	virtual int GetSweepPoints(int chan);

	enum SweepShape
	{
		//Ramp up and down
		SWEEP_SHAPE_TRIANGLE,

		//Ramp up, then jump down
		SWEEP_SHAPE_SAWTOOTH
	};

	/**
		@brief Gets the shape of a sweep

		@param chan		Zero-based channel index
	 */
	virtual SweepShape GetSweepShape(int chan);

	/**
		@brief Sets the shape of a sweep

		@param chan		Zero-based channel index
		@param shape	Shape of the sweep curve to use
	 */
	virtual void SetSweepShape(int chan, SweepShape shape);

	enum SweepSpacing
	{
		SWEEP_SPACING_LINEAR,
		SWEEP_SPACING_LOG
	};

	/**
		@brief Gets the spacing of a sweep (log or linear)

		@param chan		Zero-based channel index
	 */
	virtual SweepSpacing GetSweepSpacing(int chan);

	/**
		@brief Sets the spacing of a sweep (log or linear)

		@param chan		Zero-based channel index
		@param shape	Spacing of the sweep curve to use
	 */
	virtual void SetSweepSpacing(int chan, SweepSpacing shape);

	enum SweepDirection
	{
		SWEEP_DIR_FWD,
		SWEEP_DIR_REV
	};

	/**
		@brief Gets the direction of a sweep

		@param chan		Zero-based channel index
	 */
	virtual SweepDirection GetSweepDirection(int chan);

	/**
		@brief Sets the direction of a sweep

		@param chan		Zero-based channel index
		@param dir	Direction of the sweep curve to use
	 */
	virtual void SetSweepDirection(int chan, SweepDirection dir);
};

#endif
