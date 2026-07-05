/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of SoundCardOscilloscope

	@ingroup scopedrivers
 */

#include "scopehal.h"
#include "OscilloscopeChannel.h"
#include "SoundCardOscilloscope.h"

#include <portaudio.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Initialize the driver

	@param transport	SCPINullTransport since this driver does not connect to physical hardware
 */
SoundCardOscilloscope::SoundCardOscilloscope(SCPITransport* transport)
	: SCPIDevice(transport, false)
	, SCPIInstrument(transport, false)
	, m_extTrigger(nullptr)
{
	for(int i=0; i<4; i++)
	{
		m_rng[i] = new minstd_rand(m_rd());
		m_source[i] = new TestWaveformSource(*m_rng[i]);
	}

	m_model = "Audio Capture";
	m_vendor = "Generic";
	m_serial = "";

	PaError pa_err = Pa_Initialize();
	if(pa_err==paNoError){
		LogDebug("SoundCardOscilloscope: PortAudio initialised\n");
	}else{
		LogError("SoundCardOscilloscope: PortAudio init failed with error: %d\n",pa_err);
	}

	
	PaDeviceIndex numPADevices = Pa_GetDeviceCount();
	PaHostApiIndex nApiAvailable = Pa_GetHostApiCount();
	
	LogDebug("SoundCardOscilloscope: enumerating %d devices\n",numPADevices);

	

	int discarded = 0;

	for(int i = 0; i<numPADevices;i++){
		PaDeviceInfo* pa_dev = (PaDeviceInfo*)Pa_GetDeviceInfo(i);
		if(pa_dev == nullptr){
			discarded++;
			continue;
		}
		if(pa_dev->maxInputChannels > 0){
			// only keep devices with inputs in our list
			pa_devices.push_back(pa_dev);
		}else{
			discarded++;
		}
		
	}
	if(discarded > 0 ) LogDebug("SoundCardOscilloscope: discarded %d devices with zero inputs.\n",discarded);
	
	LogDebug("SoundCardOscilloscope: kept %zu devices which have inputs.\n",pa_devices.size());

	for(const auto& device: pa_devices){
		
		LogDebug("PA device: %s\n\tInput Channels: %d\n\tdefaultLowOutputLatency: %f\n\tdefaultHighInputLatency: %f\n\tdefaultSampleRate %f\n",
			device->name,device->maxInputChannels,device->defaultLowInputLatency,device->defaultHighInputLatency,device->defaultSampleRate);
	
		if(device->hostApi > 0 && device->hostApi < nApiAvailable){
			LogDebug("\tAPI: %s\n",Pa_GetHostApiInfo(device->hostApi)->name);
		}
	}

	

	// This doesn't work. Pa_HostApiDeviceIndexToDeviceIndex() seems to return invalid indices
	/*
	PaHostApiIndex nApiAvailable = Pa_GetHostApiCount();
	if(nApiAvailable == 0){
		LogError("SoundCardOscilloscope: no APIs available\n");
		return;
	}

	LogDebug("SoundCardOscillloscope: enumerating %d APIs. \n",nApiAvailable);
	for(int i = 0; i<nApiAvailable;i++){
		auto ApiInfo = Pa_GetHostApiInfo(i);
		LogDebug("PA API: %s\n\t has %d devices, default input device: %d\n",ApiInfo->name,ApiInfo->deviceCount,ApiInfo->defaultInputDevice);

		// enumerate the devices in this API
		for(int ii = 0; ii<ApiInfo->deviceCount;ii++){
			// from the index into the API's devices, get the global index
			PaDeviceIndex pa_idx = Pa_HostApiDeviceIndexToDeviceIndex(ApiInfo->type,ii);

			PaDeviceInfo* pa_dev = (PaDeviceInfo*)Pa_GetDeviceInfo(pa_idx);
			
			if(pa_dev->maxInputChannels > 0){
				LogDebug("\tPA device (API idx %d): %s\n\tInput Channels: %d\n\t\tdefaultLowOutputLatency: %f\n\t\tdefaultHighInputLatency: %f\n\t\tdefaultSampleRate %f\n",
					ii,pa_dev->name,pa_dev->maxInputChannels,pa_dev->defaultLowInputLatency,pa_dev->defaultHighInputLatency,pa_dev->defaultSampleRate);
			}else{
				LogDebug("\tPA device (API idx %d) skipped due to having no inputs\n",ii);
			}



		}
	}
	*/
 

	//Create a bunch of channels
	static const char* colors[8] =
	{ "#ffff00", "#ff6abc", "#00ffff", "#00c100", "#d7ffd7", "#8482ff", "#ff0000", "#ff8000" };

	for(size_t i=0; i<4; i++)
	{
		m_channels.push_back(
			new OscilloscopeChannel(
				this,
				string("CH") + to_string(i+1),
				colors[i],
				Unit(Unit::UNIT_FS),
				Unit(Unit::UNIT_VOLTS),
				Stream::STREAM_TYPE_ANALOG,
				i));

		//initial configuration is 1V p-p for each
		m_channelsEnabled[i] = true;
		m_channelCoupling[i] = OscilloscopeChannel::COUPLE_DC_50;
		m_channelAttenuation[i] = 10;
		m_channelBandwidth[i] = 0;
		m_channelVoltageRange[i] = 1;
		m_channelOffset[i] = 0;

		m_channelModes[i] = CHANNEL_MODE_NOISE_LPF;
	}

	m_sweepFreq = 1e9;

	//Default sampling configuration
	m_depth = 1e6;
	m_rate = 50e9;

	m_channels[0]->SetDisplayName("Tone");
	m_channels[1]->SetDisplayName("Ramp");
	m_channels[2]->SetDisplayName("PRBS31");
	m_channels[3]->SetDisplayName("8B10B");

	//Create Vulkan objects for the waveform conversion
	InitVulkanQueue("SoundCardOscilloscope");
}

SoundCardOscilloscope::~SoundCardOscilloscope()
{
	Pa_Terminate();
	LogDebug("Shutting down demo scope\n");

	for(int i=0; i<4; i++)
	{
		delete m_source[i];
		delete m_rng[i];
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Information queries

string SoundCardOscilloscope::IDPing()
{
	return "";
}

string SoundCardOscilloscope::GetTransportName()
{
	return "null";
}

string SoundCardOscilloscope::GetTransportConnectionString()
{
	return "";
}

///@brief Return the constant driver name "demopsu"
string SoundCardOscilloscope::GetDriverNameInternal()
{
	return "SoundCard";
}

unsigned int SoundCardOscilloscope::GetInstrumentTypes() const
{
	return INST_OSCILLOSCOPE;
}

uint32_t SoundCardOscilloscope::GetInstrumentTypesForChannel([[maybe_unused]] size_t i) const
{
	return Instrument::INST_OSCILLOSCOPE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triggering

Oscilloscope::TriggerMode SoundCardOscilloscope::PollTrigger()
{
	if(m_triggerArmed)
		return TRIGGER_MODE_TRIGGERED;
	else
		return TRIGGER_MODE_STOP;
}

void SoundCardOscilloscope::StartSingleTrigger()
{
	m_triggerArmed = true;
	m_triggerOneShot = true;
}

void SoundCardOscilloscope::Start()
{
	m_triggerArmed = true;
	m_triggerOneShot = false;
}

void SoundCardOscilloscope::Stop()
{
	m_triggerArmed = false;
	m_triggerOneShot = false;
}

void SoundCardOscilloscope::ForceTrigger()
{
	StartSingleTrigger();
}

bool SoundCardOscilloscope::IsTriggerArmed()
{
	return m_triggerArmed;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

void SoundCardOscilloscope::LoadConfiguration(int version, const YAML::Node& node, IDTable& table)
{
	//Load the channels
	auto& chans = node["channels"];
	for(auto it : chans)
	{
		auto& cnode = it.second;

		//Allocate channel space if we didn't have it yet
		size_t index = cnode["index"].as<int>();
		if(m_channels.size() < (index+1))
			m_channels.resize(index+1);

		//Configure the channel
		Stream::StreamType type = Stream::STREAM_TYPE_PROTOCOL;
		string stype = cnode["type"].as<string>();
		if(stype == "analog")
			type = Stream::STREAM_TYPE_ANALOG;
		else if(stype == "digital")
			type = Stream::STREAM_TYPE_DIGITAL;
		else if(stype == "trigger")
			type = Stream::STREAM_TYPE_TRIGGER;
		auto chan = new OscilloscopeChannel(
			this,
			cnode["name"].as<string>(),
			cnode["color"].as<string>(),
			Unit(Unit::UNIT_FS),
			Unit(Unit::UNIT_VOLTS),
			type,
			index);
		m_channels[index] = chan;

		//Create the channel ID
		table.emplace(cnode["id"].as<int>(), chan);
	}

	//Call the base class to configure everything
	Oscilloscope::LoadConfiguration(version, node, table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Channel configuration. Mostly trivial stubs.

bool SoundCardOscilloscope::IsChannelEnabled(size_t i)
{
	return m_channelsEnabled[i];
}

void SoundCardOscilloscope::EnableChannel(size_t i)
{
	m_channelsEnabled[i] = true;
}

void SoundCardOscilloscope::DisableChannel(size_t i)
{
	m_channelsEnabled[i] = false;
}

OscilloscopeChannel::CouplingType SoundCardOscilloscope::GetChannelCoupling(size_t i)
{
	return m_channelCoupling[i];
}

vector<OscilloscopeChannel::CouplingType> SoundCardOscilloscope::GetAvailableCouplings(size_t /*i*/)
{
	vector<OscilloscopeChannel::CouplingType> ret;
	ret.push_back(OscilloscopeChannel::COUPLE_DC_50);
	return ret;
}

void SoundCardOscilloscope::SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type)
{
	m_channelCoupling[i] = type;
}

double SoundCardOscilloscope::GetChannelAttenuation(size_t i)
{
	return m_channelAttenuation[i];
}

void SoundCardOscilloscope::SetChannelAttenuation(size_t i, double atten)
{
	m_channelAttenuation[i] = atten;
}

unsigned int SoundCardOscilloscope::GetChannelBandwidthLimit(size_t i)
{
	return m_channelBandwidth[i];
}

void SoundCardOscilloscope::SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz)
{
	m_channelBandwidth[i] = limit_mhz;
}

float SoundCardOscilloscope::GetChannelVoltageRange(size_t i, size_t /*stream*/)
{
	return m_channelVoltageRange[i];
}

void SoundCardOscilloscope::SetChannelVoltageRange(size_t i, size_t /*stream*/, float range)
{
	m_channelVoltageRange[i] = range;
}

bool SoundCardOscilloscope::IsHighRateOffsetCapable([[maybe_unused]] size_t i)
{
	return true;
}

OscilloscopeChannel* SoundCardOscilloscope::GetExternalTrigger()
{
	return m_extTrigger;
}

float SoundCardOscilloscope::GetChannelOffset(size_t i, size_t /*stream*/)
{
	return m_channelOffset[i];
}

void SoundCardOscilloscope::SetChannelOffset(size_t i, size_t /*stream*/, float offset)
{
	m_channelOffset[i] = offset;
}

vector<uint64_t> SoundCardOscilloscope::GetSampleRatesNonInterleaved()
{
	uint64_t k = 1000;
	uint64_t m = k * k;
	uint64_t g = k * m;

	vector<uint64_t> ret;
	ret.push_back(1 * g);
	ret.push_back(5 * g);
	ret.push_back(10 * g);
	ret.push_back(25 * g);
	ret.push_back(50 * g);
	ret.push_back(100 * g);
	ret.push_back(200 * g);
	ret.push_back(500 * g);
	return ret;
}

vector<uint64_t> SoundCardOscilloscope::GetSampleRatesInterleaved()
{
	//no-op
	vector<uint64_t> ret;
	return ret;
}

set<Oscilloscope::InterleaveConflict> SoundCardOscilloscope::GetInterleaveConflicts()
{
	//no-op
	set<Oscilloscope::InterleaveConflict> ret;
	return ret;
}

vector<uint64_t> SoundCardOscilloscope::GetSampleDepthsNonInterleaved()
{
	uint64_t k = 1000;
	uint64_t m = k * k;

	vector<uint64_t> ret;
	ret.push_back(10 * k);
	ret.push_back(100 * k);
	ret.push_back(1 * m);
	ret.push_back(10 * m);
	ret.push_back(50 * m);
	return ret;
}

vector<uint64_t> SoundCardOscilloscope::GetSampleDepthsInterleaved()
{
	//no-op
	vector<uint64_t> ret;
	return ret;
}

uint64_t SoundCardOscilloscope::GetSampleRate()
{
	return m_rate;
}

uint64_t SoundCardOscilloscope::GetSampleDepth()
{
	return m_depth;
}

void SoundCardOscilloscope::SetSampleDepth(uint64_t depth)
{
	m_depth = depth;
}

void SoundCardOscilloscope::SetSampleRate(uint64_t rate)
{
	m_rate = rate;
}

void SoundCardOscilloscope::SetTriggerOffset(int64_t /*offset*/)
{
	//FIXME
}

int64_t SoundCardOscilloscope::GetTriggerOffset()
{
	//FIXME
	return 0;
}

bool SoundCardOscilloscope::CanInterleave()
{
	return false;
}

bool SoundCardOscilloscope::IsInterleaving()
{
	return false;
}

bool SoundCardOscilloscope::SetInterleaving([[maybe_unused]] bool combine)
{
	return false;
}

void SoundCardOscilloscope::PushTrigger()
{
	//no-op
}

void SoundCardOscilloscope::PullTrigger()
{
	//no-op
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Waveform degradation modes

vector<string> SoundCardOscilloscope::GetADCModeNames(size_t /*channel*/)
{
	vector<string> ret;
	ret.push_back("Ideal");
	ret.push_back("10 mV noise");
	ret.push_back("10 mV noise + 300mm ISI channel");
	return ret;
}

size_t SoundCardOscilloscope::GetADCMode(size_t channel)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return m_channelModes[channel];
}

void SoundCardOscilloscope::SetADCMode(size_t channel, size_t mode)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_channelModes[channel] = mode;
}

bool SoundCardOscilloscope::IsADCModeConfigurable()
{
	return true;
}

bool SoundCardOscilloscope::IsADCModePerChannel()
{
	return true;
}

vector<Oscilloscope::AnalogBank> SoundCardOscilloscope::GetAnalogBanks()
{
	vector<AnalogBank> ret;
	for(size_t i=0; i<GetChannelCount(); i++)
		ret.push_back(GetAnalogBank(i));
	return ret;
}

Oscilloscope::AnalogBank SoundCardOscilloscope::GetAnalogBank(size_t channel)
{
	AnalogBank bank;
	bank.push_back(GetOscilloscopeChannel(channel));
	return bank;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Waveform synthesis

bool SoundCardOscilloscope::AcquireData()
{
	if(!m_triggerArmed)
		return false;

	// prepare all channels to be 'about to download'
	ChannelsDownloadStarted();

	//Sweeping frequency
	m_sweepFreq += 1e6;
	if(m_sweepFreq > 1.5e9)
		m_sweepFreq = 1.1e9;
	float sweepPeriod = FS_PER_SECOND / m_sweepFreq;

	//Signal degradations
	float noise[4] =
	{
		0.01, 0.01, 0.01, 0.01
	};
	bool lpf2 = false;
	bool lpf3 = false;
	{
		lock_guard<recursive_mutex> lock(m_mutex);
		for(size_t i=0; i<4; i++)
		{
			if(m_channelModes[i] == CHANNEL_MODE_IDEAL)
				noise[i] = 0;
		}
		if(m_channelModes[2] == CHANNEL_MODE_NOISE_LPF)
			lpf2 = true;
		if(m_channelModes[3] == CHANNEL_MODE_NOISE_LPF)
			lpf3 = true;
	}

	//Generate waveforms

	auto depth = GetSampleDepth();
	int64_t sampleperiod = FS_PER_SECOND / m_rate;
	WaveformBase* waveforms[4] = {nullptr};
	for(int i=0; i<4; i++)
	{
		if(!m_channelsEnabled[i])
			continue;

		// Lambda passed to generate waveform methods to update "download" percentage
		ChannelsDownloadStatusUpdate(i, InstrumentChannel::DownloadState::DOWNLOAD_IN_PROGRESS, 0.0);
		switch(i)
		{
			case 0:
				{
					auto wfm = AllocateAnalogWaveform("NoisySine");
					waveforms[i] = wfm;
					m_source[i]->GenerateNoisySinewave(
						*m_cmdBuf, m_queue, wfm, 0.9, 0.0, 1e6, sampleperiod, depth, noise[0]);
				}
				break;

			case 1:
				{
					auto wfm = AllocateAnalogWaveform("NoisySineSum");
					waveforms[i] = wfm;
					m_source[i]->GenerateNoisySinewaveSum(
						*m_cmdBuf, m_queue, wfm, 0.9, 0.0, M_PI_4, 1e6, sweepPeriod, sampleperiod, depth, noise[1]);
				}
				break;

			case 2:
				{
					auto wfm = AllocateAnalogWaveform("PRBS31");
					waveforms[i] = wfm;
					m_source[i]->GeneratePRBS31(
						*m_cmdBuf, m_queue, wfm, 0.9, 96969.6, sampleperiod, depth, lpf2, noise[2]);
				}
				break;

			case 3:
				{
					auto wfm = AllocateAnalogWaveform("8B10B");
					waveforms[i] = wfm;
					m_source[i]->Generate8b10b(
						*m_cmdBuf, m_queue, wfm, 0.9, 800e3, sampleperiod, depth, lpf3, noise[3]);
				}
				break;

			default:
				break;
		}

		ChannelsDownloadStatusUpdate(i, InstrumentChannel::DownloadState::DOWNLOAD_FINISHED, 1.0);
	}

	SequenceSet s;
	for(int i=0; i<4; i++)
		s[GetOscilloscopeChannel(i)] = waveforms[i];

	//Timestamp the waveform(s)
	double now = GetTime();
	time_t start = now;
	double tfrac = now - start;
	int64_t fs = tfrac * FS_PER_SECOND;
	for(auto it : s)
	{
		auto wfm = it.second;
		if(!wfm)
			continue;

		wfm->m_startTimestamp = start;
		wfm->m_startFemtoseconds = fs;
		wfm->m_triggerPhase = 0;
	}

	m_pendingWaveformsMutex.lock();
	m_pendingWaveforms.push_back(s);
	m_pendingWaveformsMutex.unlock();

	if(m_triggerOneShot)
		m_triggerArmed = false;

	// Tell the download monitor that waveform download has finished
	ChannelsDownloadFinished();

	return true;
}

