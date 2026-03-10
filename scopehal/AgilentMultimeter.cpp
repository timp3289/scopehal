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
#include <string>

#include "scopehal.h"
#include "AgilentMultimeter.h"
#include "MultimeterChannel.h"

using namespace std;

// Implemented
// Basic measurements


// Not implemented
// Triggering
// Sampling multiple values
// Period, 4W ohms
// Configuring digits or NPLC
// Configuring bandwidth
// Configuring input impedance

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

	// Need to be in remote mode to do anything useful
	m_transport->SendCommand("SYST:REM");

	// Clear errors
	// TODO make this smarter
	m_transport->SendCommandQueuedWithReply("SYST:ERR?");
	m_transport->SendCommandQueuedWithReply("SYST:ERR?");
	m_transport->SendCommandQueuedWithReply("SYST:ERR?");

	// Set speed to high
	//m_transport->SendCommandQueued("RATE F");

	// Rate limit to 65 Hz (which is the highest measurement supported by the metter)
	// TODO: work out actual limit for this meter - copied from Owon
	m_transport->EnableRateLimiting(chrono::milliseconds(1000/65));
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
	return AC_RMS_AMPLITUDE | DC_VOLTAGE | DC_CURRENT | AC_CURRENT | RESISTANCE | CONTINUITY | DIODE | FREQUENCY;
	// TODO: PERIOD when supported by Multimeter class
	// TODO: OHMS 4W
	// TODO: DCV RATIO when supported
}

unsigned int AgilentMultimeter::GetSecondaryMeasurementTypes()
{
	// 34401A has no secondary measurement capability
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
	return 7;
}


bool AgilentMultimeter::GetMeterAutoRange()
{
	// TODO
	// Auto-range is per measurement type

    return false;
}

void AgilentMultimeter::SetMeterAutoRange(bool enable)
{
	//TODO
	// Auto-range is per measurement type
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
	string value;
	while(true)
	{
		value = Trim(m_transport->SendCommandQueuedWithReply("READ?"));
		if(value.empty())
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
	}
}

double AgilentMultimeter::GetSecondaryMeterValue()
{
	// 34401A has no secondary measurement capability
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
	//TODO cache this
	/*
	auto s_modeReplyRaw = m_transport->SendCommandQueuedWithReply("FUNC?");
	auto s_modeReply = TrimQuotes(Trim(s_modeReplyRaw));

	// We expect a reply like:
	// "VOLT +1.000000E+02,+1.000000E-04"
	// mode, range, resolution

	string del = " ";
	
	// Split with the space to get a string with the mode
	if(!s_modeReply.empty()){
		auto pos = s_modeReply.find(del);
		if(pos!=std::string::npos){
			auto smode = s_modeReply.substr(0,pos);
			
			if(smode == "VOLT:AC")
				m_mode = AC_RMS_AMPLITUDE;
			else if(smode == "VOLT")
				m_mode = DC_VOLTAGE;
			else if(smode == "CURR:AC")
				m_mode = AC_CURRENT;
			else if(smode == "CURR")
				m_mode = DC_CURRENT;
			else if(smode == "FREQ")
				m_mode = FREQUENCY;
			else if(smode == "CONT")
				m_mode = CONTINUITY;
			else if(smode == "DIOD")
				m_mode = DIODE;
			else if(smode == "RES")
				m_mode = RESISTANCE;
			//unknown, pick something
			else
			{
				LogWarning("Unknow mode = '%s'\n", smode.c_str());
				m_mode = NONE;
			}

			return m_mode;


		}else{
			// Could not find a space in the reply
			m_mode = NONE;
			LogWarning("Failed to parse status = '%s'\n", s_modeReply.c_str());
		}
		
	}else{
		// bad SCPI reply or no SCPI reply
		m_mode = NONE;
	}
	
	return m_mode;

	//TODO caching
	//m_modeValid = true;*/

	return NONE;
	
}

Multimeter::MeasurementTypes AgilentMultimeter::GetSecondaryMeterMode()
{
	// 34401A has no secondary measurement capability
    return Multimeter::MeasurementTypes::NONE;
}

void AgilentMultimeter::SetMeterMode(Multimeter::MeasurementTypes type)
{
	/*switch(type)
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

		case FREQUENCY:
			m_transport->SendCommandImmediate("CONF:FREQ");
			break;

		case DIODE:
			m_transport->SendCommandImmediate("CONF:DIOD");
			break;

		case CONTINUITY:
			m_transport->SendCommandImmediate("CONF:CONT");
			break;

		//whatever it is, not supported
		default:
			LogWarning("Unexpected multimeter mode = '%d'", type);
			return;
	}

	m_mode = type;*/

}

void AgilentMultimeter::SetSecondaryMeterMode(Multimeter::MeasurementTypes type)
{
	// 34401A has no secondary measurement capability
	return;
}
