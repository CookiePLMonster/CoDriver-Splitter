#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <Windows.h>
#include <wrl.h>

// From DXSDK, if include directories for this .cpp are invalid, it'll throw a compilation error
#include <initguid.h>
#include <xaudio2.h>
#undef INITGUID

#include <string>
#include <cassert>

#include "MOXAudio2_Common.h"
#include "MOXAudio2_Hooks.h"

#include "IID.h"

DWORD GetCommunicationsDeviceID( IXAudio2* xaudio )
{
	UINT32 count = 0;
	if ( SUCCEEDED( xaudio->GetDeviceCount( &count ) ) )
	{
		for ( UINT32 i = 0; i < count; i++ )
		{
			XAUDIO2_DEVICE_DETAILS details;
			if ( SUCCEEDED( xaudio->GetDeviceDetails( i, &details ) ) )
			{
				if ( details.Role == DefaultCommunicationsDevice )
				{
					return i;
				}
			}
		}
	}
	return 0;
}

static ULONG objectsCount = 0;

class MOXAudio2Legacy final : public IXAudio2
{
public:
	// Inherited via IXAudio2
	virtual HRESULT WINAPI QueryInterface(REFIID riid, void ** ppvInterface) override;
	virtual ULONG WINAPI AddRef(void) override;
	virtual ULONG WINAPI Release(void) override;
	virtual HRESULT WINAPI RegisterForCallbacks(IXAudio2EngineCallback * pCallback) override;
	virtual void WINAPI UnregisterForCallbacks(IXAudio2EngineCallback * pCallback) override;
	virtual HRESULT WINAPI CreateSourceVoice(IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain) override;
	virtual HRESULT WINAPI CreateSubmixVoice(IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain) override;
	virtual HRESULT WINAPI CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 DeviceIndex, const XAUDIO2_EFFECT_CHAIN *pEffectChain) override;
	virtual HRESULT WINAPI StartEngine(void) override;
	virtual void WINAPI StopEngine(void) override;
	virtual HRESULT WINAPI CommitChanges(UINT32 OperationSet) override;
	virtual void WINAPI GetPerformanceData(XAUDIO2_PERFORMANCE_DATA * pPerfData) override;
	virtual void WINAPI SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void *pReserved) override;

	virtual HRESULT WINAPI GetDeviceCount(UINT32 * pCount) override;
	virtual HRESULT WINAPI GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS * pDeviceDetails) override;
	virtual HRESULT WINAPI Initialize(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor) override;

	MOXAudio2Legacy( IXAudio2* main, IXAudio2* aux )
		: m_mainXA2( main ), m_auxXA2( aux )
	{
		InterlockedIncrement( &objectsCount );
	}

	~MOXAudio2Legacy()
	{
		m_auxXA2->Release();
		m_mainXA2->Release();

		InterlockedDecrement( &objectsCount );
	}

private:
	IXAudio2*	m_mainXA2;
	IXAudio2*	m_auxXA2;
	LONG		m_ref = 1;

	UINT32		m_mainNumChannels = 0;
	UINT32		m_auxNumChannels = 0;
	UINT32		m_mainDeviceIndex = 0;
};

class MOXAudio2LegacyMasteringVoice final : public IXAudio2MasteringVoice
{
public:
	// Inherited via IXAudio2MasteringVoice
	virtual void WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails) override;
	virtual HRESULT WINAPI SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList) override;
	virtual HRESULT WINAPI SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain) override;
	virtual HRESULT WINAPI EnableEffect(UINT32 EffectIndex, UINT32 OperationSet) override;
	virtual HRESULT WINAPI DisableEffect(UINT32 EffectIndex, UINT32 OperationSet) override;
	virtual void WINAPI GetEffectState(UINT32 EffectIndex, BOOL *pEnabled) override;
	virtual HRESULT WINAPI SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet) override;
	virtual HRESULT WINAPI GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize) override;
	virtual HRESULT WINAPI SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet) override;
	virtual void WINAPI GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters) override;
	virtual HRESULT WINAPI SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet) override;
	virtual void WINAPI GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters) override;
	virtual HRESULT WINAPI SetVolume(float Volume, UINT32 OperationSet) override;
	virtual void WINAPI GetVolume(float *pVolume) override;
	virtual HRESULT WINAPI SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet) override;
	virtual void WINAPI GetChannelVolumes(UINT32 Channels, float *pVolumes) override;
	virtual HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet) override;
	virtual void WINAPI GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix) override;
	virtual void WINAPI DestroyVoice() override;

	MOXAudio2LegacyMasteringVoice( IXAudio2MasteringVoice* main, IXAudio2MasteringVoice* aux )
		: m_mainVoice( main ), m_auxVoice( aux )
	{
	}

private:
	IXAudio2MasteringVoice* m_mainVoice;
	IXAudio2MasteringVoice* m_auxVoice;
};

class MOXAudio2LegacySourceVoice final : public IXAudio2SourceVoice
{
public:

	// Inherited via IXAudio2SourceVoice
	virtual HRESULT WINAPI Start(UINT32 Flags, UINT32 OperationSet) override;
	virtual HRESULT WINAPI Stop(UINT32 Flags, UINT32 OperationSet) override;
	virtual HRESULT WINAPI SubmitSourceBuffer(const XAUDIO2_BUFFER * pBuffer, const XAUDIO2_BUFFER_WMA *pBufferWMA) override;
	virtual HRESULT WINAPI FlushSourceBuffers(void) override;
	virtual HRESULT WINAPI Discontinuity(void) override;
	virtual HRESULT WINAPI ExitLoop(UINT32 OperationSet) override;
	virtual void WINAPI GetState(XAUDIO2_VOICE_STATE * pVoiceState) override;
	virtual HRESULT WINAPI SetFrequencyRatio(float Ratio, UINT32 OperationSet) override;
	virtual void WINAPI GetFrequencyRatio(float * pRatio) override;
	virtual HRESULT WINAPI SetSourceSampleRate(UINT32 NewSourceSampleRate) override;
	virtual void WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails) override;
	virtual HRESULT WINAPI SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList) override;
	virtual HRESULT WINAPI SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain) override;
	virtual HRESULT WINAPI EnableEffect(UINT32 EffectIndex, UINT32 OperationSet) override;
	virtual HRESULT WINAPI DisableEffect(UINT32 EffectIndex, UINT32 OperationSet) override;
	virtual void WINAPI GetEffectState(UINT32 EffectIndex, BOOL *pEnabled) override;
	virtual HRESULT WINAPI SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet) override;
	virtual HRESULT WINAPI GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize) override;
	virtual HRESULT WINAPI SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet) override;
	virtual void WINAPI GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters) override;
	virtual HRESULT WINAPI SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet) override;
	virtual void WINAPI GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters) override;
	virtual HRESULT WINAPI SetVolume(float Volume, UINT32 OperationSet) override;
	virtual void WINAPI GetVolume(float *pVolume) override;
	virtual HRESULT WINAPI SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet) override;
	virtual void WINAPI GetChannelVolumes(UINT32 Channels, float *pVolumes) override;
	virtual HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet) override;
	virtual void WINAPI GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix) override;
	virtual void WINAPI DestroyVoice() override;

	MOXAudio2LegacySourceVoice( IXAudio2SourceVoice* main, IXAudio2SourceVoice* aux, SIZE_T RingBufferSize )
		: m_mainVoice( main ), m_auxVoice( aux ), m_auxVoiceBuffer( RingBufferSize )
	{
	}

private:
	IXAudio2SourceVoice* m_mainVoice;
	IXAudio2SourceVoice* m_auxVoice;

	AuxillaryVoiceRingBuffer m_auxVoiceBuffer;
};


HRESULT WINAPI MOXAudio2Legacy::QueryInterface(REFIID riid, void ** ppvInterface)
{
	if ( ppvInterface == nullptr ) return E_POINTER;

	if ( riid == IID_IUnknown || riid == __uuidof(IXAudio2) 
		|| riid == IID_MOXAudio2 ) // Custom extension so wrappers are "self aware"
	{
		*ppvInterface = static_cast<IXAudio2*>(this);
		AddRef();
		return S_OK;
	}

	*ppvInterface = nullptr;
	return E_NOINTERFACE;
}

ULONG WINAPI MOXAudio2Legacy::AddRef(void)
{
	return InterlockedIncrement( &m_ref );
}

ULONG WINAPI MOXAudio2Legacy::Release(void)
{
	const LONG ref = InterlockedDecrement( &m_ref );
	if ( ref == 0 )
	{
		delete this;
	}
	return ref;
}

HRESULT WINAPI MOXAudio2Legacy::RegisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	m_auxXA2->RegisterForCallbacks(pCallback);
	return m_mainXA2->RegisterForCallbacks(pCallback);
}

void WINAPI MOXAudio2Legacy::UnregisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	m_auxXA2->UnregisterForCallbacks(pCallback);
	m_mainXA2->UnregisterForCallbacks(pCallback);
}

HRESULT WINAPI MOXAudio2Legacy::CreateSourceVoice(IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	// Only wrap voices with at least 3 channels
	// This never happens with supported titles, but prevents unsupported titles from crashing
	if ( pSourceFormat->nChannels < 3 )
	{
		return m_mainXA2->CreateSourceVoice(ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
	}

	IXAudio2SourceVoice* mainVoice = nullptr;
	IXAudio2SourceVoice* auxVoice = nullptr;
	HRESULT result = m_mainXA2->CreateSourceVoice(&mainVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
	m_auxXA2->CreateSourceVoice(&auxVoice, pSourceFormat, Flags, MaxFrequencyRatio, nullptr, pSendList, pEffectChain);

	*ppSourceVoice = new MOXAudio2LegacySourceVoice( mainVoice, auxVoice, CalculateAuxBufferSize( pSourceFormat ) );

	// Mute center speaker on output matrix
	std::vector<float> levels;
	const DWORD source = pSourceFormat->nChannels;


	{
		const DWORD destination = m_mainNumChannels;
		levels.resize( source * destination );

		mainVoice->GetOutputMatrix( nullptr, source, destination, levels.data() );
		SetOutputMatrixForMain( source, destination, levels );
		result = mainVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );
	}

	{
		const DWORD destination = m_auxNumChannels;
		levels.resize( source * destination );

		auxVoice->GetOutputMatrix( nullptr, source, destination, levels.data() );
		SetOutputMatrixForAuxillary( source, destination, levels );
		result = auxVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );
	}

	return result;
}

HRESULT WINAPI MOXAudio2Legacy::CreateSubmixVoice(IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	assert( !"Submix voices not supported!");
	return E_NOTIMPL;
}

HRESULT WINAPI MOXAudio2Legacy::CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 DeviceIndex, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	IXAudio2MasteringVoice* mainVoice = nullptr;
	IXAudio2MasteringVoice* auxVoice = nullptr;

	HRESULT hrAux = m_auxXA2->CreateMasteringVoice(&auxVoice, InputChannels, InputSampleRate, Flags, GetCommunicationsDeviceID(m_auxXA2), pEffectChain);
	if ( FAILED(hrAux) )
	{
		return hrAux;
	}

	HRESULT hrMain = m_mainXA2->CreateMasteringVoice(&mainVoice, InputChannels, InputSampleRate, Flags, DeviceIndex, pEffectChain);
	if ( FAILED(hrMain) )
	{
		auxVoice->DestroyVoice();
		return hrMain;
	}

	*ppMasteringVoice = new MOXAudio2LegacyMasteringVoice( mainVoice, auxVoice );

	XAUDIO2_VOICE_DETAILS details;
	mainVoice->GetVoiceDetails( &details );
	m_mainNumChannels = details.InputChannels;

	auxVoice->GetVoiceDetails( &details );
	m_auxNumChannels = details.InputChannels;

	m_mainDeviceIndex = DeviceIndex;

	return S_OK;
}

HRESULT WINAPI MOXAudio2Legacy::StartEngine(void)
{
	m_auxXA2->StartEngine();
	return m_mainXA2->StartEngine();
}

void WINAPI MOXAudio2Legacy::StopEngine(void)
{
	m_auxXA2->StopEngine();
	m_mainXA2->StopEngine();
}

HRESULT WINAPI MOXAudio2Legacy::CommitChanges(UINT32 OperationSet)
{
	m_auxXA2->CommitChanges(OperationSet);
	return m_mainXA2->CommitChanges(OperationSet);
}

void WINAPI MOXAudio2Legacy::GetPerformanceData(XAUDIO2_PERFORMANCE_DATA * pPerfData)
{
	m_auxXA2->GetPerformanceData(pPerfData);
	m_mainXA2->GetPerformanceData(pPerfData);
}

void WINAPI MOXAudio2Legacy::SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void * pReserved)
{
	m_auxXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
	m_mainXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
}

HRESULT WINAPI MOXAudio2Legacy::GetDeviceCount(UINT32 * pCount)
{
	// No need to call this on auxillary voice
	return m_mainXA2->GetDeviceCount(pCount);
}

HRESULT WINAPI MOXAudio2Legacy::GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS * pDeviceDetails)
{
	// No need to call this on auxillary voice
	HRESULT hr = m_mainXA2->GetDeviceDetails(Index, pDeviceDetails);
	if ( SUCCEEDED(hr) && Index == m_mainDeviceIndex )
	{
		FixupMasteringVoiceChannelMask( &pDeviceDetails->OutputFormat.dwChannelMask );
		pDeviceDetails->OutputFormat.Format.nChannels = static_cast<WORD>(PopCount( pDeviceDetails->OutputFormat.dwChannelMask ));
	}
	return hr;
}

HRESULT WINAPI MOXAudio2Legacy::Initialize(UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
{
	m_auxXA2->Initialize(Flags, XAudio2Processor);
	return m_mainXA2->Initialize(Flags, XAudio2Processor);
}



HRESULT WINAPI MOXAudio2LegacySourceVoice::Start(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Start(Flags, OperationSet);
	return m_mainVoice->Start(Flags, OperationSet);
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::Stop(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Stop(Flags, OperationSet);
	return m_mainVoice->Stop(Flags, OperationSet);
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::SubmitSourceBuffer(const XAUDIO2_BUFFER * pBuffer, const XAUDIO2_BUFFER_WMA * pBufferWMA)
{
	XAUDIO2_BUFFER auxBuffer = *pBuffer;
	auxBuffer.pAudioData = m_auxVoiceBuffer.CopyToRingBuffer( pBuffer->pAudioData, pBuffer->AudioBytes );

	m_auxVoice->SubmitSourceBuffer(&auxBuffer, pBufferWMA);
	return m_mainVoice->SubmitSourceBuffer(pBuffer, pBufferWMA);
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::FlushSourceBuffers(void)
{
	m_auxVoice->FlushSourceBuffers();
	return m_mainVoice->FlushSourceBuffers();
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::Discontinuity(void)
{
	m_auxVoice->Discontinuity();
	return m_mainVoice->Discontinuity();
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::ExitLoop(UINT32 OperationSet)
{
	m_auxVoice->ExitLoop(OperationSet);
	return m_mainVoice->ExitLoop(OperationSet);
}

void WINAPI MOXAudio2LegacySourceVoice::GetState(XAUDIO2_VOICE_STATE * pVoiceState)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetState(pVoiceState);
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::SetFrequencyRatio(float Ratio, UINT32 OperationSet)
{
	m_auxVoice->SetFrequencyRatio(Ratio, OperationSet);
	return m_mainVoice->SetFrequencyRatio(Ratio, OperationSet);
}

void WINAPI MOXAudio2LegacySourceVoice::GetFrequencyRatio(float * pRatio)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetFrequencyRatio(pRatio);
}

HRESULT WINAPI MOXAudio2LegacySourceVoice::SetSourceSampleRate(UINT32 NewSourceSampleRate)
{
	m_auxVoice->SetSourceSampleRate(NewSourceSampleRate);
	return m_mainVoice->SetSourceSampleRate(NewSourceSampleRate);
}

void MOXAudio2LegacySourceVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVoiceDetails(pVoiceDetails);
}

HRESULT MOXAudio2LegacySourceVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	// TODO: Translate the sends list (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputVoices(pSendList);
	return m_mainVoice->SetOutputVoices(pSendList);
}

HRESULT MOXAudio2LegacySourceVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	m_auxVoice->SetEffectChain(pEffectChain);
	return m_mainVoice->SetEffectChain(pEffectChain);
}

HRESULT MOXAudio2LegacySourceVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->EnableEffect(EffectIndex, OperationSet);
	return m_mainVoice->EnableEffect(EffectIndex, OperationSet);
}

HRESULT MOXAudio2LegacySourceVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->DisableEffect(EffectIndex, OperationSet);
	return m_mainVoice->DisableEffect(EffectIndex, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetEffectState(EffectIndex, pEnabled);
}

HRESULT MOXAudio2LegacySourceVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	m_auxVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
	return m_mainVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
}

HRESULT MOXAudio2LegacySourceVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	// No need to call this on auxillary voice
	return m_mainVoice->GetEffectParameters(EffectIndex, pParameters, ParametersByteSize);
}

HRESULT MOXAudio2LegacySourceVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetFilterParameters(pParameters, OperationSet);
	return m_mainVoice->SetFilterParameters(pParameters, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice 
	m_mainVoice->GetFilterParameters(pParameters);
}

HRESULT MOXAudio2LegacySourceVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
	return m_mainVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_mainVoice->GetOutputFilterParameters(pDestinationVoice, pParameters);
}

HRESULT MOXAudio2LegacySourceVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	m_auxVoice->SetVolume(Volume, OperationSet);
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetVolume(float * pVolume)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVolume(pVolume);
}

HRESULT MOXAudio2LegacySourceVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	m_auxVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
	return m_mainVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetChannelVolumes(Channels, pVolumes);
}

HRESULT MOXAudio2LegacySourceVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	// TODO: Apply same modifications to output matrix here as in CreateSourceVoice
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	return m_mainVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

void MOXAudio2LegacySourceVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix);
}

void MOXAudio2LegacySourceVoice::DestroyVoice()
{
	m_auxVoice->DestroyVoice();
	m_mainVoice->DestroyVoice();

	delete this;
}




void WINAPI MOXAudio2LegacyMasteringVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVoiceDetails( pVoiceDetails );
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	m_auxVoice->SetOutputVoices(pSendList);
	return m_mainVoice->SetOutputVoices(pSendList);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	m_auxVoice->SetEffectChain(pEffectChain);
	return m_mainVoice->SetEffectChain(pEffectChain);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->EnableEffect(EffectIndex, OperationSet);
	return m_mainVoice->EnableEffect(EffectIndex, OperationSet);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->DisableEffect(EffectIndex, OperationSet);
	return m_mainVoice->DisableEffect(EffectIndex, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetEffectState(EffectIndex, pEnabled);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	m_auxVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
	return m_mainVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	// No need to call this on auxillary voice
	return m_mainVoice->GetEffectParameters(EffectIndex, pParameters, ParametersByteSize);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetFilterParameters(pParameters, OperationSet);
	return m_mainVoice->SetFilterParameters(pParameters, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice 
	m_mainVoice->GetFilterParameters(pParameters);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
	return m_mainVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputFilterParameters(pDestinationVoice, pParameters);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	m_auxVoice->SetVolume(Volume, OperationSet);
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetVolume(float * pVolume)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVolume(pVolume);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	m_auxVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
	return m_mainVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetChannelVolumes(Channels, pVolumes);
}

HRESULT WINAPI MOXAudio2LegacyMasteringVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	m_auxVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	return m_mainVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

void WINAPI MOXAudio2LegacyMasteringVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix);
}

void WINAPI MOXAudio2LegacyMasteringVoice::DestroyVoice()
{
	m_auxVoice->DestroyVoice();
	m_mainVoice->DestroyVoice();

	delete this;
}

std::optional<HRESULT> CreateLegacyXAudio2(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
	if ( rclsid != CLSID_XAudio2 && rclsid != CLSID_XAudio2_Debug )
	{
		return {};
	}

	using namespace Microsoft::WRL;
	using namespace Microsoft::WRL::Wrappers;

	HMODULE realXAudio2 = LoadRealLegacyXAudio2( rclsid == CLSID_XAudio2_Debug );
	if ( realXAudio2 == nullptr )
	{
		return E_UNEXPECTED;
	}

	auto createFn = (HRESULT(WINAPI*)(REFCLSID,REFIID,LPVOID*))GetProcAddress( realXAudio2, "DllGetClassObject" );
	if ( createFn == nullptr )
	{
		return E_UNEXPECTED;
	}

	ComPtr<IClassFactory> factory;
	HRESULT hr = createFn( rclsid, IID_PPV_ARGS(factory.GetAddressOf()) );
	if ( FAILED(hr) )
	{
		return hr;
	}

	ComPtr<IXAudio2> mainDevice;
	hr = factory->CreateInstance( nullptr, IID_PPV_ARGS(mainDevice.GetAddressOf()) );
	if ( FAILED(hr) )
	{
		return hr;
	}

	ComPtr<MOXAudio2Legacy> moxaDevice;
	if ( SUCCEEDED( mainDevice.As( &moxaDevice ) ) )
	{
		*ppv = moxaDevice.Detach();
		return S_OK;
	}

	ComPtr<IXAudio2> auxDevice;
	hr = factory->CreateInstance( nullptr, IID_PPV_ARGS(auxDevice.GetAddressOf()) );
	if ( FAILED(hr) )
	{
		return hr;
	}

	moxaDevice.Attach( new MOXAudio2Legacy( mainDevice.Detach(), auxDevice.Detach() ) );

	// Query it to whatever the game wanted just to be extra sure
	return moxaDevice.CopyTo( riid, ppv );
}



// COM factory for MOXAudio2Legacy
class MOXAudio2LegacyFactory final : public IClassFactory
{
public:
	// Inherited via IClassFactory
	virtual HRESULT WINAPI QueryInterface(REFIID riid, void ** ppvObject) override
	{
		if ( ppvObject == nullptr ) return E_POINTER;

		if ( riid == IID_IUnknown || riid == IID_IClassFactory )
		{
			*ppvObject = static_cast<IClassFactory*>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	virtual ULONG WINAPI AddRef(void) override
	{
		return InterlockedIncrement( &m_ref );
	}

	virtual ULONG WINAPI Release(void) override
	{
		const ULONG ref = InterlockedDecrement( &m_ref );
		if ( ref == 0 && m_lock == 0 )
		{
			delete this;
		}
		return ref;
	}

	virtual HRESULT WINAPI CreateInstance(IUnknown * pUnkOuter, REFIID riid, void ** ppvObject) override
	{
		auto hr = CreateLegacyXAudio2( m_clsid, riid, ppvObject );
		return hr ? *hr : E_NOINTERFACE;
	}

	virtual HRESULT WINAPI LockServer(BOOL fLock) override
	{
		if ( fLock != FALSE )
		{
			InterlockedIncrement( &m_lock );
		}
		else
		{
			const ULONG lock = InterlockedDecrement( &m_lock );
			if ( lock == 0 && m_ref == 0 )
			{
				delete this;
			}
		}
		return S_OK;
	}

	explicit MOXAudio2LegacyFactory( REFCLSID rclsid )
		: m_clsid( rclsid )
	{
		InterlockedIncrement( &objectsCount );
	}

	~MOXAudio2LegacyFactory()
	{
		InterlockedDecrement( &objectsCount );
	}

private:
	const CLSID	m_clsid;

	ULONG	m_ref = 1;
	ULONG	m_lock = 0;
};

HRESULT DllGetClassObject_Wrap(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
	if ( rclsid == CLSID_XAudio2 || rclsid == CLSID_XAudio2_Debug )
	{
		Microsoft::WRL::ComPtr<MOXAudio2LegacyFactory> factory;
		factory.Attach( new MOXAudio2LegacyFactory( rclsid ) );
		return factory.CopyTo( riid, ppv );
	}
	return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C"
{
	HRESULT WINAPI DllGetClassObject_Export(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
	{
		return DllGetClassObject_Wrap( rclsid, riid, ppv );
	}

	HRESULT WINAPI DllCanUnloadNow_Export()
	{
		return objectsCount == 0 ? S_OK : S_FALSE;
	}
}
