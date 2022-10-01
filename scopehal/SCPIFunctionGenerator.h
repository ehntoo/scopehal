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

#ifndef SCPIFunctionGenerator_h
#define SCPIFunctionGenerator_h

#include "FunctionGenerator.h"
#include "SCPIInstrument.h"

/**
	@brief An SCPI-based function generator
 */
class SCPIFunctionGenerator 	: public virtual FunctionGenerator
						, public virtual SCPIInstrument
{
public:
	SCPIFunctionGenerator();
	virtual ~SCPIFunctionGenerator();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Dynamic creation
public:
	typedef SCPIFunctionGenerator* (*GeneratorCreateProcType)(SCPITransport*);
	static void DoAddDriverClass(std::string name, GeneratorCreateProcType proc);

	static void EnumDrivers(std::vector<std::string>& names);
	static SCPIFunctionGenerator* CreateFunctionGenerator(std::string driver, SCPITransport* transport);

	virtual std::string GetDriverName() =0;

protected:
	//Class enumeration
	typedef std::map< std::string, GeneratorCreateProcType > GeneratorCreateMapType;
	static GeneratorCreateMapType m_gencreateprocs;
};

//Use this for function generators that are not also oscilloscopes
#define GENERATOR_INITPROC(T) \
	static SCPIFunctionGenerator* CreateInstance(SCPITransport* transport) \
	{	return new T(transport); } \
	virtual std::string GetDriverName() \
	{ return GetDriverNameInternal(); }

#define AddFunctionGeneratorDriverClass(T) SCPIFunctionGenerator::DoAddDriverClass(T::GetDriverNameInternal(), T::CreateInstance)


#endif
