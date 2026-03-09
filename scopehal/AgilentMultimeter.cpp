/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal                                                                                                          *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@author Tim Pattinson
	@brief Agilent 34401A multimeter driver
 */
#include "scopehal.h"
#include "AgilentMultimeter.h"
#include "MultimeterChannel.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

AgilentMultimeter::AgilentMultimeter(SCPITransport* transport)
	: SCPIDevice(transport)
	, SCPIInstrument(transport)
//	, m_modeValid(false)
//	, m_secmodeValid(false)
{
	//prefetch operating mode
	GetMeterMode();

	//Create our single channel
	m_channels.push_back(new MultimeterChannel(this, "VIN", "#ffff00", 0));

	// Set speed to high
	//m_transport->SendCommandQueued("RATE F");

	// Rate limit to 65 Hz (which is the highest measurement supported by the metter)
	//m_transport->EnableRateLimiting(chrono::milliseconds(1000/65));
}

AgilentMultimeter::~AgilentMultimeter()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device info

string AgilentMultimeter::GetDriverNameInternal()
{
	return "agilent_dmm";
}

unsigned int AgilentMultimeter::GetInstrumentTypes() const
{
	return INST_DMM;
}

unsigned int AgilentMultimeter::GetMeasurementTypes()
{
	return AC_RMS_AMPLITUDE | DC_VOLTAGE | DC_CURRENT | AC_CURRENT | TEMPERATURE | CAPACITANCE | RESISTANCE | CONTINUITY | DIODE | FREQUENCY;
}

unsigned int AgilentMultimeter::GetSecondaryMeasurementTypes()
{
	/*switch(m_mode)
	{
		case AC_RMS_AMPLITUDE:
		case AC_CURRENT:
			return FREQUENCY;

		default:
			return 0;
	}*/
    return 0;
}

uint32_t AgilentMultimeter::GetInstrumentTypesForChannel([[maybe_unused]] size_t i) const
{
	return INST_DMM;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMM mode

int AgilentMultimeter::GetMeterDigits()
{
	return 5;
}

bool AgilentMultimeter::GetMeterAutoRange()
{
	//string reply = m_transport->SendCommandQueuedWithReply("AUTO?");
	//return (Trim(reply) == "1");
    return false;
}

void AgilentMultimeter::SetMeterAutoRange(bool enable)
{
	/*if(enable)
		m_transport->SendCommandImmediate("AUTO");
	else
	{
		m_transport->SendCommandImmediate("RANGE 1");
	}*/
}

void AgilentMultimeter::StartMeter()
{
	//cannot be started or stopped
}

void AgilentMultimeter::StopMeter()
{
	//cannot be started or stopped
}

double AgilentMultimeter::GetMeterValue()
{
	/*string value;
	while(true)
	{
		value = Trim(m_transport->SendCommandQueuedWithReply("MEAS1?"));
		if(value.empty()||(value.find("NON")!=std::string::npos))
		{
			LogWarning("Failed to read value: got '%s'\n",value.c_str());
			continue;
		}
		else if(value == "1E+9") // Overload
			return std::numeric_limits<double>::max();

		istringstream os(value);
    	double result;
    	os >> result;
		return result;
	}*/
    return 123.45;
}

double AgilentMultimeter::GetSecondaryMeterValue()
{
	/*if(GetSecondaryMeterMode()==NONE)
		return 0;
	//If we have a secondary value, this gets it
	//If no secondary mode configured, returns primary value
	string value;
	while(true)
	{
		value = Trim(m_transport->SendCommandQueuedWithReply("MEAS2?"));
		if((value.find("NON")!=std::string::npos))
		{
			// No secondary reading at this point
			return 0;
		}
		else if(value == "1E+9")
			return std::numeric_limits<double>::max(); // Overload
		if(value.empty())
		{
			LogWarning("Failed to read value: got '%s'\n",value.c_str());
			continue;
		}
		istringstream os(value);
    	double result;
    	os >> result;
		return result;
	}*/
    return 0.0;
}

int AgilentMultimeter::GetCurrentMeterChannel()
{
	return 0;
}

void AgilentMultimeter::SetCurrentMeterChannel([[maybe_unused] ]int chan)
{
	//nop
}

Multimeter::MeasurementTypes AgilentMultimeter::GetMeterMode()
{
	/*if(m_modeValid)
		return m_mode;

	auto smode = TrimQuotes(Trim(m_transport->SendCommandQueuedWithReply("FUNC?")));

	//Default to no alternate mode
	m_secmode = NONE;


	if(smode == "VOLT AC")
		m_mode = AC_RMS_AMPLITUDE;
	else if(smode == "VOLT")
		m_mode = DC_VOLTAGE;
	else if(smode == "CURR AC")
		m_mode = AC_CURRENT;
	else if(smode == "CURR")
		m_mode = DC_CURRENT;
	else if(smode == "FREQ")
		m_mode = FREQUENCY;
	else if(smode == "FREQ")
		m_mode = FREQUENCY;
	else if(smode == "CAP")
		m_mode = CAPACITANCE;
	else if(smode == "CONT")
		m_mode = CONTINUITY;
	else if(smode == "DIOD")
		m_mode = DIODE;
	else if(smode == "RES")
		m_mode = RESISTANCE;
	else if(smode == "TEMP")
		m_mode = TEMPERATURE;


	//unknown, pick something
	else
	{
		LogWarning("Unknow mode = '%s', defaulting to DC Voltage\n", smode.c_str());
		m_mode = DC_VOLTAGE;
	}

	// Get secondary measure
	smode = TrimQuotes(Trim(m_transport->SendCommandQueuedWithReply("FUNC2?")));

	if(smode.find("FREQ")!=std::string::npos)
		m_secmode = FREQUENCY;
	else
		m_secmode = NONE;


	m_modeValid = true;
	m_secmodeValid = true;
	return m_mode;*/
    return Multimeter::MeasurementTypes::NONE;
}

Multimeter::MeasurementTypes AgilentMultimeter::GetSecondaryMeterMode()
{
	/*if(m_secmodeValid)
		return m_secmode;

	GetMeterMode();
	return m_secmode;*/
    return Multimeter::MeasurementTypes::NONE;
}

void AgilentMultimeter::SetMeterMode(Multimeter::MeasurementTypes type)
{
	/*m_secmode = NONE;

	switch(type)
	{
		case DC_VOLTAGE:
			m_transport->SendCommandImmediate("CONF:VOLT:DC");
			break;

		case AC_RMS_AMPLITUDE:
			m_transport->SendCommandImmediate("CONF:VOLT:AC");
			break;

		case DC_CURRENT:
			m_transport->SendCommandImmediate("CONF:CURR:DC");
			break;

		case AC_CURRENT:
			m_transport->SendCommandImmediate("CONF:CURR:AC");
			break;

		case RESISTANCE:
			m_transport->SendCommandImmediate("CONF:RES");
			break;

		case CAPACITANCE:
			m_transport->SendCommandImmediate("CONF:CAP");
			break;

		case FREQUENCY:
			m_transport->SendCommandImmediate("CONF:FREQ");
			break;

		case DIODE:
			m_transport->SendCommandImmediate("CONF:DIOD");
			break;

		case CONTINUITY:
			m_transport->SendCommandImmediate("CONF:CONT");
			break;

		case TEMPERATURE:
			m_transport->SendCommandImmediate("CONF:TEMP");
			break;

		//whatever it is, not supported
		default:
			return;
	}

	m_mode = type;
	if(m_mode != AC_CURRENT && m_mode != AC_RMS_AMPLITUDE)
		m_secmode = NONE;*/
}

void AgilentMultimeter::SetSecondaryMeterMode(Multimeter::MeasurementTypes type)
{
	/*
    switch(type)
	{
		case FREQUENCY:
			m_transport->SendCommandImmediate("FUNC2 \"FREQ\"");
			break;

		case NONE:
			m_transport->SendCommandImmediate("FUNC2 \"NONe\"");
			break;

		//not supported
		default:
			return;
	}

	m_secmode = type;
	m_secmodeValid = true;
    */
}
