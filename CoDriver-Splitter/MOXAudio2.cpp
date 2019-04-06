#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <Windows.h>

#include <xaudio2.h>
#include <Shlwapi.h>
#include <string>

#include <wrl.h>
#include <Mmdeviceapi.h>

#include <vector>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")

#define INITGUID
#include <guiddef.h>
DEFINE_GUID(DEVINTERFACE_AUDIO_RENDER, 0xe6327cad, 0xdcec, 0x4949, 0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2);
#undef INITGUID


HMODULE hRealXAudio2;
static void LoadRealXAudio2()
{
	if ( hRealXAudio2 != nullptr ) return;

	TCHAR		wcSystemPath[MAX_PATH];
	GetSystemDirectory(wcSystemPath, MAX_PATH);
	PathAppend(wcSystemPath, XAUDIO2_DLL);

	hRealXAudio2 = LoadLibrary( wcSystemPath );
}

std::wstring GetCommunicationsDeviceString()
{
	using namespace Microsoft::WRL;
	using namespace Microsoft::WRL::Wrappers;

	std::wstring result;

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

	return result;
}

void FixupMasteringVoiceChannelMask( DWORD* pChannelmask )
{
	// Fixup rules, backed by testing in DiRT Rally:
	// - We need at least 4 speakers for the game to emit 5.1 audio
	// - We will always add front left/right and center speakers, and additionally add side speakers if back speakers are not present
	*pChannelmask |= SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT|SPEAKER_FRONT_CENTER;
	if ( (*pChannelmask & (SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_FRONT_RIGHT_OF_CENTER | SPEAKER_BACK_LEFT|SPEAKER_BACK_RIGHT)) == 0 )
	{
		*pChannelmask |= SPEAKER_SIDE_LEFT|SPEAKER_SIDE_RIGHT;
	}
}

DWORD PopCount( DWORD mask )
{
	DWORD count = 0;
	DWORD testMask = 1;
	for ( size_t i = 0; i < 32; i++ )
	{
		if ( (mask & testMask) != 0 )
		{
			count++;
		}
		testMask <<= 1;
	}
	return count;
}

void NormalizeOutputMatrix( std::vector<float>& levels, float volume )
{
	float max = *std::max_element( levels.begin(), levels.end() );	
	if ( max != 0.0f )
	{
		// Make sure this will not make the most prominent channel quieter
		max /= volume;
		if ( max < 1.0f )
		{
			for ( auto& value : levels )
			{
				value /= max;
			}
		}
	}
}

class MOXAudio2 final : public IXAudio2
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
		m_mainXA2->AddRef();
		m_auxXA2->AddRef();
	}

	~MOXAudio2()
	{
		m_auxXA2->Release();
		m_mainXA2->Release();
	}

private:
	IXAudio2*	m_mainXA2;
	IXAudio2*	m_auxXA2;
	LONG		m_ref = 1;

	DWORD		m_mainNumChannels = 0;
	DWORD		m_auxNumChannels = 0;
};

class MOXAudio2MasteringVoice final : public IXAudio2MasteringVoice
{
public:
	// Inherited via IXAudio2MasteringVoice
	virtual HRESULT WINAPI GetChannelMask(DWORD * pChannelmask) override;
	virtual void WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails);
	virtual HRESULT WINAPI SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList);
	virtual HRESULT WINAPI SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain);
	virtual HRESULT WINAPI EnableEffect(UINT32 EffectIndex, UINT32 OperationSet);
	virtual HRESULT WINAPI DisableEffect(UINT32 EffectIndex, UINT32 OperationSet);
	virtual void WINAPI GetEffectState(UINT32 EffectIndex, BOOL *pEnabled);
	virtual HRESULT WINAPI SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet);
	virtual HRESULT WINAPI GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize);
	virtual HRESULT WINAPI SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet);
	virtual void WINAPI GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet);
	virtual void WINAPI GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetVolume(float Volume, UINT32 OperationSet);
	virtual void WINAPI GetVolume(float *pVolume);
	virtual HRESULT WINAPI SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet);
	virtual void WINAPI GetChannelVolumes(UINT32 Channels, float *pVolumes);
	virtual HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet);
	virtual void WINAPI GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix);
	virtual void WINAPI DestroyVoice();

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
	virtual void WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails);
	virtual HRESULT WINAPI SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList);
	virtual HRESULT WINAPI SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain);
	virtual HRESULT WINAPI EnableEffect(UINT32 EffectIndex, UINT32 OperationSet);
	virtual HRESULT WINAPI DisableEffect(UINT32 EffectIndex, UINT32 OperationSet);
	virtual void WINAPI GetEffectState(UINT32 EffectIndex, BOOL *pEnabled);
	virtual HRESULT WINAPI SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet);
	virtual HRESULT WINAPI GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize);
	virtual HRESULT WINAPI SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet);
	virtual void WINAPI GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet);
	virtual void WINAPI GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetVolume(float Volume, UINT32 OperationSet);
	virtual void WINAPI GetVolume(float *pVolume);
	virtual HRESULT WINAPI SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet);
	virtual void WINAPI GetChannelVolumes(UINT32 Channels, float *pVolumes);
	virtual HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet);
	virtual void WINAPI GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix);
	virtual void WINAPI DestroyVoice();

	MOXAudio2SourceVoice( IXAudio2SourceVoice* main, IXAudio2SourceVoice* aux )
		: m_mainVoice( main ), m_auxVoice( aux )
	{
	}

private:
	IXAudio2SourceVoice* m_mainVoice;
	IXAudio2SourceVoice* m_auxVoice;
};

extern "C"
{

__declspec(dllexport) HRESULT WINAPI MOXAudio2Create( IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor )
{
	LoadRealXAudio2();

	auto createFn = (HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR ))GetProcAddress( hRealXAudio2, "XAudio2Create" );

	IXAudio2* mainDevice = nullptr;
	IXAudio2* auxDevice = nullptr;
	HRESULT result1 = createFn( &mainDevice, Flags, XAudio2Processor );
	HRESULT result2 = createFn( &auxDevice, Flags, XAudio2Processor );

	*ppXAudio2 = new MOXAudio2( mainDevice, auxDevice );
	return result1;
}

}



HRESULT WINAPI MOXAudio2::QueryInterface(REFIID riid, void ** ppvInterface)
{
	if ( ppvInterface == nullptr ) return E_POINTER;

	if ( riid == IID_IUnknown || riid == __uuidof(IXAudio2) )
	{
		*ppvInterface = static_cast<IXAudio2*>(this);
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
	const LONG ref = InterlockedDecrement( &m_ref );
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
	IXAudio2SourceVoice* mainVoice = nullptr;
	IXAudio2SourceVoice* auxVoice = nullptr;
	HRESULT result = m_mainXA2->CreateSourceVoice(&mainVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
	m_auxXA2->CreateSourceVoice(&auxVoice, pSourceFormat, Flags, MaxFrequencyRatio, nullptr, pSendList, pEffectChain);

	// TODO: Manage this pointer
	*ppSourceVoice = new MOXAudio2SourceVoice( mainVoice, auxVoice );

	// Mute center speaker on output matrix
	std::vector<float> levels;
	const DWORD source = pSourceFormat->nChannels;
	auto getOutput = [&]( size_t src, size_t dest ) -> float& {
		return levels[ source * dest + src ];
	};


	{
		const DWORD destination = m_mainNumChannels;
		levels.resize( source * destination );

		mainVoice->GetOutputMatrix( nullptr, source, destination, levels.data() );

		for ( DWORD i = 0; i < destination; i++ )
		{
			getOutput( 2, i ) = 0.0f;
		}
		// If downmixing, give environment a volume boost by normalizing channels
		if ( source > destination )
		{
			NormalizeOutputMatrix( levels, 0.5f );
		}

		result = mainVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );
	}

	{
		const DWORD destination = m_auxNumChannels;
		levels.resize( source * destination );

		auxVoice->GetOutputMatrix( nullptr, source, destination, levels.data() );
		for ( DWORD i = 0; i < destination; i++ )
		{
			for ( DWORD j = 0; j < source; j++ )
			{
				if ( j != 2 )
				{
					getOutput( j, i ) = 0.0f;
				}
			}
		}
		// We want the co-driver to be easily audible - if downmixing, give it a volume boost by normalizing channels
		if ( source > destination )
		{
			NormalizeOutputMatrix( levels, 0.75f );
		}

		result = auxVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );
	}

	return result;
}

HRESULT WINAPI MOXAudio2::CreateSubmixVoice(IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	m_auxXA2->CreateSubmixVoice(ppSubmixVoice, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
	return m_mainXA2->CreateSubmixVoice(ppSubmixVoice, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
}

HRESULT WINAPI MOXAudio2::CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, LPCWSTR szDeviceId, const XAUDIO2_EFFECT_CHAIN * pEffectChain, AUDIO_STREAM_CATEGORY StreamCategory)
{
	IXAudio2MasteringVoice* mainVoice = nullptr;
	IXAudio2MasteringVoice* auxVoice = nullptr;

	HRESULT hrAux = m_auxXA2->CreateMasteringVoice(&auxVoice, InputChannels, InputSampleRate, Flags, GetCommunicationsDeviceString().c_str(), pEffectChain, StreamCategory);
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

	// TODO: Manage this pointer
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
	m_auxVoice->SubmitSourceBuffer(pBuffer, pBufferWMA);
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
