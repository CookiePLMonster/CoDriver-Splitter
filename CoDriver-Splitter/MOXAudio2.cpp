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

#include <array>

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

class MOXAudio2 final : public IXAudio2
{
public:
	// Inherited via IXAudio2
	virtual HRESULT WINAPI QueryInterface(REFIID riid, void ** ppvInterface) override;
	virtual ULONG WINAPI AddRef(void) override;
	virtual ULONG WINAPI Release(void) override;
	virtual HRESULT WINAPI RegisterForCallbacks(IXAudio2EngineCallback * pCallback) override;
	virtual void WINAPI UnregisterForCallbacks(IXAudio2EngineCallback * pCallback) override;
	virtual HRESULT WINAPI CreateSourceVoice(IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags X2DEFAULT(0), float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO), IXAudio2VoiceCallback *pCallback X2DEFAULT(NULL), const XAUDIO2_VOICE_SENDS *pSendList X2DEFAULT(NULL), const XAUDIO2_EFFECT_CHAIN *pEffectChain X2DEFAULT(NULL)) override;
	virtual HRESULT WINAPI CreateSubmixVoice(IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0), const XAUDIO2_VOICE_SENDS *pSendList X2DEFAULT(NULL), const XAUDIO2_EFFECT_CHAIN *pEffectChain X2DEFAULT(NULL)) override;
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

private:
	IXAudio2*	m_mainXA2;
	IXAudio2*	m_auxXA2;
	LONG		m_ref = 1;
};

class MOXAudio2MasteringVoice final : public IXAudio2MasteringVoice
{
public:
	// Inherited via IXAudio2MasteringVoice
	virtual HRESULT WINAPI GetChannelMask(DWORD * pChannelmask) override;
	virtual void WINAPI GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails);
	virtual HRESULT WINAPI SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList);
	virtual HRESULT WINAPI SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain);
	virtual HRESULT WINAPI EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = 0U);
	virtual HRESULT WINAPI DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = 0U);
	virtual void WINAPI GetEffectState(UINT32 EffectIndex, BOOL *pEnabled);
	virtual HRESULT WINAPI SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet = 0U);
	virtual HRESULT WINAPI GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize);
	virtual HRESULT WINAPI SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet = 0U);
	virtual void WINAPI GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet = 0U);
	virtual void WINAPI GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters);
	virtual HRESULT WINAPI SetVolume(float Volume, UINT32 OperationSet = 0U);
	virtual void WINAPI GetVolume(float *pVolume);
	virtual HRESULT WINAPI SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet = 0U);
	virtual void WINAPI GetChannelVolumes(UINT32 Channels, float *pVolumes);
	virtual HRESULT WINAPI SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet = 0U);
	virtual void WINAPI GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix);
	virtual void WINAPI DestroyVoice();

	MOXAudio2MasteringVoice( IXAudio2MasteringVoice* main, IXAudio2MasteringVoice* aux )
		: m_mainVoice( main ), m_auxVoice( aux )
	{
	}

private:
	IXAudio2MasteringVoice* m_mainVoice;
	IXAudio2MasteringVoice* m_auxVoice;
	LONG m_ref = 1;
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
	LONG m_ref = 1;
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

// Inherited via IXAudio2

inline HRESULT __stdcall MOXAudio2::QueryInterface(REFIID riid, void ** ppvInterface)
{
	*ppvInterface = this;
	return S_OK;
}

inline ULONG __stdcall MOXAudio2::AddRef(void)
{
	return 0;
}

inline ULONG __stdcall MOXAudio2::Release(void)
{
	return 0;
}

inline HRESULT __stdcall MOXAudio2::RegisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	HRESULT hr = m_mainXA2->RegisterForCallbacks(pCallback);
	m_auxXA2->RegisterForCallbacks(pCallback);
	return hr;
}

inline void __stdcall MOXAudio2::UnregisterForCallbacks(IXAudio2EngineCallback * pCallback)
{
	m_mainXA2->UnregisterForCallbacks(pCallback);
	m_auxXA2->UnregisterForCallbacks(pCallback);
}

inline HRESULT __stdcall MOXAudio2::CreateSourceVoice(IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	IXAudio2SourceVoice* mainVoice = nullptr;
	IXAudio2SourceVoice* auxVoice = nullptr;
	HRESULT result = m_mainXA2->CreateSourceVoice(&mainVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, pSendList, pEffectChain);
	m_auxXA2->CreateSourceVoice(&auxVoice, pSourceFormat, Flags, MaxFrequencyRatio, nullptr, pSendList, pEffectChain);

	// TODO: Manage this pointer
	*ppSourceVoice = new MOXAudio2SourceVoice( mainVoice, auxVoice );

	// Mute center speaker on output matrix
	const size_t source = 6;
	const size_t destination = 2;
	std::array<float, source * destination> levels = {};
	auto getOutput = [&]( size_t src, size_t dest ) -> float& {
		return levels[ source * dest + src ];
	};

	mainVoice->GetOutputMatrix( nullptr, source, destination, levels.data() );

	for ( size_t i = 0; i < destination; i++ )
	{
		getOutput( 2, i ) = 0.0f;
	}

	mainVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );

	levels.fill( 0.0f );
	for ( size_t i = 0; i < destination; i++ )
	{
		getOutput( 2, i ) = 1.0f;
	}
	auxVoice->SetOutputMatrix( nullptr, source, destination, levels.data() );

	return result;
}

inline HRESULT __stdcall MOXAudio2::CreateSubmixVoice(IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS *pSendList, const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
	return m_mainXA2->CreateSubmixVoice(ppSubmixVoice, InputChannels, InputSampleRate, Flags, ProcessingStage, pSendList, pEffectChain);
}

inline HRESULT __stdcall MOXAudio2::CreateMasteringVoice(IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags, LPCWSTR szDeviceId, const XAUDIO2_EFFECT_CHAIN * pEffectChain, AUDIO_STREAM_CATEGORY StreamCategory)
{
	IXAudio2MasteringVoice* mainVoice = nullptr;
	IXAudio2MasteringVoice* auxVoice = nullptr;
	HRESULT hrMain = m_mainXA2->CreateMasteringVoice(&mainVoice, InputChannels, InputSampleRate, Flags, szDeviceId, pEffectChain, StreamCategory);
	m_auxXA2->CreateMasteringVoice(&auxVoice, InputChannels, InputSampleRate, Flags, GetCommunicationsDeviceString().c_str(), pEffectChain, StreamCategory);

	// TODO: Manage this pointer
	*ppMasteringVoice = new MOXAudio2MasteringVoice( mainVoice, auxVoice );

	return hrMain;
}

inline HRESULT __stdcall MOXAudio2::StartEngine(void)
{
	m_auxXA2->StartEngine();
	return m_mainXA2->StartEngine();
}

inline void __stdcall MOXAudio2::StopEngine(void)
{
	m_auxXA2->StopEngine();
	m_mainXA2->StopEngine();
}

inline HRESULT __stdcall MOXAudio2::CommitChanges(UINT32 OperationSet)
{
	m_auxXA2->CommitChanges(OperationSet);
	return m_mainXA2->CommitChanges(OperationSet);
}

inline void __stdcall MOXAudio2::GetPerformanceData(XAUDIO2_PERFORMANCE_DATA * pPerfData)
{
	m_auxXA2->GetPerformanceData(pPerfData);
	m_mainXA2->GetPerformanceData(pPerfData);
}

inline void __stdcall MOXAudio2::SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void * pReserved)
{
	m_auxXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
	m_mainXA2->SetDebugConfiguration(pDebugConfiguration, pReserved);
}

// Inherited via IXAudio2SourceVoice

inline HRESULT __stdcall MOXAudio2SourceVoice::Start(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Start(Flags, OperationSet);
	return m_mainVoice->Start(Flags, OperationSet);
}

inline HRESULT __stdcall MOXAudio2SourceVoice::Stop(UINT32 Flags, UINT32 OperationSet)
{
	m_auxVoice->Stop(Flags, OperationSet);
	return m_mainVoice->Stop(Flags, OperationSet);
}

inline HRESULT __stdcall MOXAudio2SourceVoice::SubmitSourceBuffer(const XAUDIO2_BUFFER * pBuffer, const XAUDIO2_BUFFER_WMA * pBufferWMA)
{
	m_auxVoice->SubmitSourceBuffer(pBuffer, pBufferWMA);
	return m_mainVoice->SubmitSourceBuffer(pBuffer, pBufferWMA);
}

inline HRESULT __stdcall MOXAudio2SourceVoice::FlushSourceBuffers(void)
{
	m_auxVoice->FlushSourceBuffers();
	return m_mainVoice->FlushSourceBuffers();
}

inline HRESULT __stdcall MOXAudio2SourceVoice::Discontinuity(void)
{
	m_auxVoice->Discontinuity();
	return m_mainVoice->Discontinuity();
}

inline HRESULT __stdcall MOXAudio2SourceVoice::ExitLoop(UINT32 OperationSet)
{
	// TODO
	return m_mainVoice->ExitLoop(OperationSet);
}

inline void __stdcall MOXAudio2SourceVoice::GetState(XAUDIO2_VOICE_STATE * pVoiceState, UINT32 Flags)
{
	// TODO
	m_mainVoice->GetState(pVoiceState, Flags);
}

inline HRESULT __stdcall MOXAudio2SourceVoice::SetFrequencyRatio(float Ratio, UINT32 OperationSet)
{
	// TODO
	return m_mainVoice->SetFrequencyRatio(Ratio, OperationSet);
}

inline void __stdcall MOXAudio2SourceVoice::GetFrequencyRatio(float * pRatio)
{
	// TODO
	m_mainVoice->GetFrequencyRatio(pRatio);
}

inline HRESULT __stdcall MOXAudio2SourceVoice::SetSourceSampleRate(UINT32 NewSourceSampleRate)
{
	// TODO
	return m_mainVoice->SetSourceSampleRate(NewSourceSampleRate);
}

inline void MOXAudio2SourceVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// TODO
	m_mainVoice->GetVoiceDetails(pVoiceDetails);
}

inline HRESULT MOXAudio2SourceVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	return E_NOTIMPL;
}

inline HRESULT MOXAudio2SourceVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	return E_NOTIMPL;
}

inline HRESULT MOXAudio2SourceVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline HRESULT MOXAudio2SourceVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void MOXAudio2SourceVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
}

inline HRESULT MOXAudio2SourceVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline HRESULT MOXAudio2SourceVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	return E_NOTIMPL;
}

inline HRESULT MOXAudio2SourceVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void MOXAudio2SourceVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
}

inline HRESULT MOXAudio2SourceVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void MOXAudio2SourceVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
}

inline HRESULT MOXAudio2SourceVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	m_auxVoice->SetVolume(Volume, OperationSet);
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

inline void MOXAudio2SourceVoice::GetVolume(float * pVolume)
{
	// TODO
	m_mainVoice->GetVolume(pVolume);
}

inline HRESULT MOXAudio2SourceVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void MOXAudio2SourceVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
}

inline HRESULT MOXAudio2SourceVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void MOXAudio2SourceVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
}

inline void MOXAudio2SourceVoice::DestroyVoice()
{
	// TODO
	m_auxVoice->DestroyVoice();
	m_mainVoice->DestroyVoice();
}


// Inherited via IXAudio2MasteringVoice

inline HRESULT __stdcall MOXAudio2MasteringVoice::GetChannelMask(DWORD * pChannelmask)
{
	DWORD mask = 63;
	//HRESULT hr = m_interface->GetChannelMask( &mask );
	*pChannelmask = mask | SPEAKER_FRONT_CENTER;
	return S_OK;
}

inline void __stdcall MOXAudio2MasteringVoice::GetVoiceDetails(XAUDIO2_VOICE_DETAILS * pVoiceDetails)
{
	// TODO
	m_mainVoice->GetVoiceDetails( pVoiceDetails );
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetOutputVoices(const XAUDIO2_VOICE_SENDS * pSendList)
{
	// TODO
	return m_mainVoice->SetOutputVoices(pSendList);
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetEffectChain(const XAUDIO2_EFFECT_CHAIN * pEffectChain)
{
	return m_mainVoice->SetEffectChain(pEffectChain);
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::EnableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	return m_mainVoice->EnableEffect(EffectIndex, OperationSet);
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::DisableEffect(UINT32 EffectIndex, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void __stdcall MOXAudio2MasteringVoice::GetEffectState(UINT32 EffectIndex, BOOL * pEnabled)
{
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetEffectParameters(UINT32 EffectIndex, const void * pParameters, UINT32 ParametersByteSize, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::GetEffectParameters(UINT32 EffectIndex, void * pParameters, UINT32 ParametersByteSize)
{
	return E_NOTIMPL;
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void __stdcall MOXAudio2MasteringVoice::GetFilterParameters(XAUDIO2_FILTER_PARAMETERS * pParameters)
{
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void __stdcall MOXAudio2MasteringVoice::GetOutputFilterParameters(IXAudio2Voice * pDestinationVoice, XAUDIO2_FILTER_PARAMETERS * pParameters)
{
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetVolume(float Volume, UINT32 OperationSet)
{
	return m_mainVoice->SetVolume(Volume, OperationSet);
}

inline void __stdcall MOXAudio2MasteringVoice::GetVolume(float * pVolume)
{
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetChannelVolumes(UINT32 Channels, const float * pVolumes, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void __stdcall MOXAudio2MasteringVoice::GetChannelVolumes(UINT32 Channels, float * pVolumes)
{
}

inline HRESULT __stdcall MOXAudio2MasteringVoice::SetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet)
{
	return E_NOTIMPL;
}

inline void __stdcall MOXAudio2MasteringVoice::GetOutputMatrix(IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float * pLevelMatrix)
{
}

inline void __stdcall MOXAudio2MasteringVoice::DestroyVoice()
{
}
