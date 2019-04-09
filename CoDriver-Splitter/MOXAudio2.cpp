#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0602
#define _WIN32_WINNT 0x0602
#include <Windows.h>
#include <wrl.h>

#include <xaudio2.h>
#include <string>
#include <cassert>

#include <initguid.h>
#include <Mmdeviceapi.h>
#undef INITGUID

#include "MOXAudio2_Common.h"
#include "MOXAudio2_Hooks.h"

#include "MOXAudio2.h"

std::wstring GetCommunicationsDeviceString()
{
	using namespace Microsoft::WRL;
	using namespace Microsoft::WRL::Wrappers;

	std::wstring result;

	HRESULT hrCom = CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );

	// Superfluous scope to ensure COM pointer deinits before CoUnitialize
	{
		ComPtr<IMMDeviceEnumerator> enumerator;

		HRESULT hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator), NULL,
			CLSCTX_ALL, IID_PPV_ARGS(enumerator.GetAddressOf()) );
		if ( SUCCEEDED(hr) )
		{
			ComPtr<IMMDevice> device;
			hr = enumerator->GetDefaultAudioEndpoint( eRender, eCommunications, device.GetAddressOf() );
			if ( SUCCEEDED(hr) )
			{
				LPWSTR strId = nullptr;
				if ( SUCCEEDED( device->GetId( &strId ) ) )
				{
					// Sources:
					// https://github.com/citizenfx/fivem/blob/e628cfa2e0a4e9e803de31af3c805e9baba57048/code/components/voip-mumble/src/MumbleAudioOutput.cpp#L710
					// https://gist.github.com/mendsley/fbb495b292b95d35a014109e586d35dd
					result.reserve(112);
					result.append(L"\\\\?\\SWD#MMDEVAPI#");
					result.append(strId);
					result.push_back(L'#');
					const size_t offset = result.size();

					result.resize(result.capacity());
					StringFromGUID2(DEVINTERFACE_AUDIO_RENDER, &result[offset], (int)(result.size() - offset));
					CoTaskMemFree( strId );
				}
			}
		}
	}

	if ( SUCCEEDED(hrCom) )
	{
		CoUninitialize();
	}

	return result;
}
class MOXAudio2 final : public IXAudio2, public IMOXAudio2
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
	virtual HRESULT WINAPI CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, LPCWSTR szDeviceId, const XAUDIO2_EFFECT_CHAIN *pEffectChain, AUDIO_STREAM_CATEGORY StreamCategory) override;
	virtual HRESULT WINAPI StartEngine(void) override;
	virtual void WINAPI StopEngine(void) override;
	virtual HRESULT WINAPI CommitChanges(UINT32 OperationSet) override;
	virtual void WINAPI GetPerformanceData(XAUDIO2_PERFORMANCE_DATA * pPerfData) override;
	virtual void WINAPI SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void *pReserved) override;

	MOXAudio2( IXAudio2* main, IXAudio2* aux )
		: m_mainXA2( main ), m_auxXA2( aux )
	{
	}

	~MOXAudio2()
	{
		m_auxXA2->Release();
		m_mainXA2->Release();
	}

private:
	IXAudio2*	m_mainXA2;
	IXAudio2*	m_auxXA2;
	ULONG		m_ref = 1;

	DWORD		m_mainNumChannels = 0;
	DWORD		m_auxNumChannels = 0;
};

class MOXAudio2MasteringVoice final : public IXAudio2MasteringVoice
{
public:
	// Inherited via IXAudio2MasteringVoice
	virtual HRESULT WINAPI GetChannelMask(DWORD * pChannelmask) override;
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

	MOXAudio2MasteringVoice( IXAudio2MasteringVoice* main, IXAudio2MasteringVoice* aux )
		: m_mainVoice( main ), m_auxVoice( aux )
	{
	}

private:
	IXAudio2MasteringVoice* m_mainVoice;
	IXAudio2MasteringVoice* m_auxVoice;
};

class MOXAudio2SourceVoice final : public IXAudio2SourceVoice
{
public:

	// Inherited via IXAudio2SourceVoice
	virtual HRESULT WINAPI Start(UINT32 Flags, UINT32 OperationSet) override;
	virtual HRESULT WINAPI Stop(UINT32 Flags, UINT32 OperationSet) override;
	virtual HRESULT WINAPI SubmitSourceBuffer(const XAUDIO2_BUFFER * pBuffer, const XAUDIO2_BUFFER_WMA *pBufferWMA) override;
	virtual HRESULT WINAPI FlushSourceBuffers(void) override;
	virtual HRESULT WINAPI Discontinuity(void) override;
	virtual HRESULT WINAPI ExitLoop(UINT32 OperationSet) override;
	virtual void WINAPI GetState(XAUDIO2_VOICE_STATE * pVoiceState, UINT32 Flags) override;
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

	MOXAudio2SourceVoice( IXAudio2SourceVoice* main, IXAudio2SourceVoice* aux, SIZE_T RingBufferSize )
		: m_mainVoice( main ), m_auxVoice( aux ), m_auxVoiceBuffer( RingBufferSize )
	{
	}

private:
	IXAudio2SourceVoice* m_mainVoice;
	IXAudio2SourceVoice* m_auxVoice;

	AuxillaryVoiceRingBuffer m_auxVoiceBuffer;
};


HRESULT WINAPI MOXAudio2::QueryInterface(REFIID riid, void ** ppvInterface)
{
	if ( ppvInterface == nullptr ) return E_POINTER;

	if ( riid == IID_IUnknown || riid == __uuidof(IXAudio2) )
	{
		*ppvInterface = static_cast<IXAudio2*>(this);
		AddRef();
		return S_OK;
	}

	if ( riid == __uuidof(IMOXAudio2) )
	{
		*ppvInterface = static_cast<IMOXAudio2*>(this);
		AddRef();
		return S_OK;
	}

	*ppvInterface = nullptr;
	return E_NOINTERFACE;
}

ULONG WINAPI MOXAudio2::AddRef(void)
{
	return InterlockedIncrement( &m_ref );
}

ULONG WINAPI MOXAudio2::Release(void)
{
	const ULONG ref = InterlockedDecrement( &m_ref );
	if ( ref == 0 )
	{
		delete this;
	}
	return ref;
}

HRESULT WINAPI MOXAudio2::RegisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	m_auxXA2->RegisterForCallbacks(pCallback);
	return m_mainXA2->RegisterForCallbacks(pCallback);
}

void WINAPI MOXAudio2::UnregisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	m_auxXA2->UnregisterForCallbacks(pCallback);
	m_mainXA2->UnregisterForCallbacks(pCallback);
}

HRESULT WINAPI MOXAudio2::CreateSourceVoice(IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
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

	*ppSourceVoice = new MOXAudio2SourceVoice( mainVoice, auxVoice, CalculateAuxBufferSize( pSourceFormat ) );

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

HRESULT WINAPI MOXAudio2::CreateSubmixVoice(IXAudio2SubmixVoice ** /*ppSubmixVoice*/, UINT32 /*InputChannels*/, UINT32 /*InputSampleRate*/, UINT32 /*Flags*/, UINT32 /*ProcessingStage*/, const XAUDIO2_VOICE_SENDS * /*pSendList*/, const XAUDIO2_EFFECT_CHAIN * /*pEffectChain*/)
{
	assert( !"Submix voices not supported!");
	return E_NOTIMPL;
}

HRESULT WINAPI MOXAudio2::CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, LPCWSTR szDeviceId, const XAUDIO2_EFFECT_CHAIN * pEffectChain, AUDIO_STREAM_CATEGORY StreamCategory)
{
	IXAudio2MasteringVoice* mainVoice = nullptr;
	IXAudio2MasteringVoice* auxVoice = nullptr;

	HRESULT hrAux = m_auxXA2->CreateMasteringVoice(&auxVoice, InputChannels, InputSampleRate, Flags, GetCommunicationsDeviceString().c_str(), pEffectChain, AudioCategory_Speech);
	if ( FAILED(hrAux) )
	{
		return hrAux;
	}

	HRESULT hrMain = m_mainXA2->CreateMasteringVoice(&mainVoice, InputChannels, InputSampleRate, Flags, szDeviceId, pEffectChain, StreamCategory);
	if ( FAILED(hrMain) )
	{
		auxVoice->DestroyVoice();
		return hrMain;
	}

	*ppMasteringVoice = new MOXAudio2MasteringVoice( mainVoice, auxVoice );

	DWORD mask;
	if ( SUCCEEDED( mainVoice->GetChannelMask( &mask ) ) )
	{
		m_mainNumChannels = PopCount( mask );
	}
	if ( SUCCEEDED( auxVoice->GetChannelMask( &mask ) ) )
	{
		m_auxNumChannels = PopCount( mask );
	}

	return S_OK;
}

HRESULT WINAPI MOXAudio2::StartEngine(void)
{
	m_auxXA2->StartEngine();
	return m_mainXA2->StartEngine();
}

void WINAPI MOXAudio2::StopEngine(void)
{
	m_auxXA2->StopEngine();
	m_mainXA2->StopEngine();
}

HRESULT WINAPI MOXAudio2::CommitChanges(UINT32 OperationSet)
{
	m_auxXA2->CommitChanges(OperationSet);
	return m_mainXA2->CommitChanges(OperationSet);
}

void WINAPI MOXAudio2::GetPerformanceData(XAUDIO2_PERFORMANCE_DATA * pPerfData)
{
	m_auxXA2->GetPerformanceData(pPerfData);
	m_mainXA2->GetPerformanceData(pPerfData);
}

void WINAPI MOXAudio2::SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void * pReserved)
{
	m_auxXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
	m_mainXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
}



HRESULT WINAPI MOXAudio2SourceVoice::Start(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Start(Flags, OperationSet);
	return m_mainVoice->Start(Flags, OperationSet);
}

HRESULT WINAPI MOXAudio2SourceVoice::Stop(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Stop(Flags, OperationSet);
	return m_mainVoice->Stop(Flags, OperationSet);
}

HRESULT WINAPI MOXAudio2SourceVoice::SubmitSourceBuffer(const XAUDIO2_BUFFER * pBuffer, const XAUDIO2_BUFFER_WMA * pBufferWMA)
{
	XAUDIO2_BUFFER auxBuffer = *pBuffer;
	auxBuffer.pAudioData = m_auxVoiceBuffer.CopyToRingBuffer( pBuffer->pAudioData, pBuffer->AudioBytes );

	m_auxVoice->SubmitSourceBuffer(&auxBuffer, pBufferWMA);
	return m_mainVoice->SubmitSourceBuffer(pBuffer, pBufferWMA);
}

HRESULT WINAPI MOXAudio2SourceVoice::FlushSourceBuffers(void)
{
	m_auxVoice->FlushSourceBuffers();
	return m_mainVoice->FlushSourceBuffers();
}

HRESULT WINAPI MOXAudio2SourceVoice::Discontinuity(void)
{
	m_auxVoice->Discontinuity();
	return m_mainVoice->Discontinuity();
}

HRESULT WINAPI MOXAudio2SourceVoice::ExitLoop(UINT32 OperationSet)
{
	m_auxVoice->ExitLoop(OperationSet);
	return m_mainVoice->ExitLoop(OperationSet);
}

void WINAPI MOXAudio2SourceVoice::GetState(XAUDIO2_VOICE_STATE * pVoiceState, UINT32 Flags)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetState(pVoiceState, Flags);
}

HRESULT WINAPI MOXAudio2SourceVoice::SetFrequencyRatio(float Ratio, UINT32 OperationSet)
{
	m_auxVoice->SetFrequencyRatio(Ratio, OperationSet);
	return m_mainVoice->SetFrequencyRatio(Ratio, OperationSet);
}

void WINAPI MOXAudio2SourceVoice::GetFrequencyRatio(float * pRatio)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetFrequencyRatio(pRatio);
}

HRESULT WINAPI MOXAudio2SourceVoice::SetSourceSampleRate(UINT32 NewSourceSampleRate)
{
	m_auxVoice->SetSourceSampleRate(NewSourceSampleRate);
	return m_mainVoice->SetSourceSampleRate(NewSourceSampleRate);
}

void MOXAudio2SourceVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVoiceDetails(pVoiceDetails);
	pVoiceDetails->CreationFlags |= MOXAUDIO2_VOICE_MULTIOUTPUT;
}

HRESULT MOXAudio2SourceVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	// TODO: Translate the sends list (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputVoices(pSendList);
	return m_mainVoice->SetOutputVoices(pSendList);
}

HRESULT MOXAudio2SourceVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	m_auxVoice->SetEffectChain(pEffectChain);
	return m_mainVoice->SetEffectChain(pEffectChain);
}

HRESULT MOXAudio2SourceVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->EnableEffect(EffectIndex, OperationSet);
	return m_mainVoice->EnableEffect(EffectIndex, OperationSet);
}

HRESULT MOXAudio2SourceVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->DisableEffect(EffectIndex, OperationSet);
	return m_mainVoice->DisableEffect(EffectIndex, OperationSet);
}

void MOXAudio2SourceVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetEffectState(EffectIndex, pEnabled);
}

HRESULT MOXAudio2SourceVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	m_auxVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
	return m_mainVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
}

HRESULT MOXAudio2SourceVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	// No need to call this on auxillary voice
	return m_mainVoice->GetEffectParameters(EffectIndex, pParameters, ParametersByteSize);
}

HRESULT MOXAudio2SourceVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetFilterParameters(pParameters, OperationSet);
	return m_mainVoice->SetFilterParameters(pParameters, OperationSet);
}

void MOXAudio2SourceVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice 
	m_mainVoice->GetFilterParameters(pParameters);
}

HRESULT MOXAudio2SourceVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
	return m_mainVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
}

void MOXAudio2SourceVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_mainVoice->GetOutputFilterParameters(pDestinationVoice, pParameters);
}

HRESULT MOXAudio2SourceVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	m_auxVoice->SetVolume(Volume, OperationSet);
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

void MOXAudio2SourceVoice::GetVolume(float * pVolume)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVolume(pVolume);
}

HRESULT MOXAudio2SourceVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	m_auxVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
	return m_mainVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
}

void MOXAudio2SourceVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetChannelVolumes(Channels, pVolumes);
}

HRESULT MOXAudio2SourceVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	// TODO: Apply same modifications to output matrix here as in CreateSourceVoice
	// TODO: Handle IXAudio2Voice properly (use RTTI to determine whether it's submix or mastering maybe?)
	m_auxVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	return m_mainVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

void MOXAudio2SourceVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix);
}

void MOXAudio2SourceVoice::DestroyVoice()
{
	m_auxVoice->DestroyVoice();
	m_mainVoice->DestroyVoice();

	delete this;
}




HRESULT WINAPI MOXAudio2MasteringVoice::GetChannelMask(DWORD * pChannelmask)
{
	HRESULT hr = m_mainVoice->GetChannelMask( pChannelmask );
	if ( SUCCEEDED(hr) )
	{
		FixupMasteringVoiceChannelMask( pChannelmask );
	}
	return hr;
}

void WINAPI MOXAudio2MasteringVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVoiceDetails( pVoiceDetails );
	pVoiceDetails->CreationFlags |= MOXAUDIO2_VOICE_MULTIOUTPUT;
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	m_auxVoice->SetOutputVoices(pSendList);
	return m_mainVoice->SetOutputVoices(pSendList);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	m_auxVoice->SetEffectChain(pEffectChain);
	return m_mainVoice->SetEffectChain(pEffectChain);
}

HRESULT WINAPI MOXAudio2MasteringVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->EnableEffect(EffectIndex, OperationSet);
	return m_mainVoice->EnableEffect(EffectIndex, OperationSet);
}

HRESULT WINAPI MOXAudio2MasteringVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	m_auxVoice->DisableEffect(EffectIndex, OperationSet);
	return m_mainVoice->DisableEffect(EffectIndex, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetEffectState(EffectIndex, pEnabled);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	m_auxVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
	return m_mainVoice->SetEffectParameters(EffectIndex, pParameters, ParametersByteSize, OperationSet);
}

HRESULT WINAPI MOXAudio2MasteringVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	// No need to call this on auxillary voice
	return m_mainVoice->GetEffectParameters(EffectIndex, pParameters, ParametersByteSize);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetFilterParameters(pParameters, OperationSet);
	return m_mainVoice->SetFilterParameters(pParameters, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice 
	m_mainVoice->GetFilterParameters(pParameters);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	m_auxVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
	return m_mainVoice->SetOutputFilterParameters(pDestinationVoice, pParameters, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputFilterParameters(pDestinationVoice, pParameters);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	m_auxVoice->SetVolume(Volume, OperationSet);
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetVolume(float * pVolume)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetVolume(pVolume);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	m_auxVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
	return m_mainVoice->SetChannelVolumes(Channels, pVolumes, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetChannelVolumes(Channels, pVolumes);
}

HRESULT WINAPI MOXAudio2MasteringVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	m_auxVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
	return m_mainVoice->SetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

void WINAPI MOXAudio2MasteringVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
	// No need to call this on auxillary voice
	m_mainVoice->GetOutputMatrix(pDestinationVoice, SourceChannels, DestinationChannels, pLevelMatrix);
}

void WINAPI MOXAudio2MasteringVoice::DestroyVoice()
{
	m_auxVoice->DestroyVoice();
	m_mainVoice->DestroyVoice();

	delete this;
}


HRESULT WINAPI MOXAudio2Create( IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor )
{
	HMODULE realXAudio2 = LoadRealXAudio2();
	if ( realXAudio2 == nullptr )
	{
		return XAUDIO2_E_INVALID_CALL;
	}

	auto createFn = (HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR ))GetProcAddress( realXAudio2, "XAudio2Create" );
	if ( createFn == nullptr )
	{
		return XAUDIO2_E_INVALID_CALL;
	}

	using namespace Microsoft::WRL;
	using namespace Microsoft::WRL::Wrappers;

	ComPtr<IXAudio2> mainDevice;
	HRESULT hrMain = createFn( mainDevice.GetAddressOf(), Flags, XAudio2Processor );
	if ( FAILED(hrMain) )
	{
		return hrMain;
	}

	// In DiRT Rally on Windows 10, it is possible that "real" XAudio2 is in fact our own XAudio2.9
	// Try to detect this case and if it's really happening, don't wrap the APIs again
	ComPtr<IMOXAudio2> maybeMOXAudio2;
	if ( SUCCEEDED( mainDevice.As( &maybeMOXAudio2 ) ) )
	{
		*ppXAudio2 = mainDevice.Detach();
		return S_OK;
	}

	ComPtr<IXAudio2> auxDevice;
	HRESULT hrAux = createFn( auxDevice.GetAddressOf(), Flags, XAudio2Processor );
	if ( FAILED(hrAux) )
	{
		return hrAux;
	}

	*ppXAudio2 = new MOXAudio2( mainDevice.Detach(), auxDevice.Detach() );
	return S_OK;
}

extern "C"
{
	HRESULT WINAPI XAudio2Create_Export( IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor )
	{
		return MOXAudio2Create( ppXAudio2, Flags, XAudio2Processor );
	}
}
