#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0601 // Opt for lowest supported version here (Win7)
#define _WIN32_WINNT 0x0601
#include <Windows.h>

#include "MOXAudio2_Common.h"

#include <wrl.h>
#include <initguid.h>
#include <Mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>

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

static void NormalizeOutputMatrix( std::vector<float>& levels, float volume )
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

void SetOutputMatrixForMain( UINT32 source, UINT32 destination, std::vector<float>& levels )
{
	auto getOutput = [&]( size_t src, size_t dest ) -> float& {
		return levels[ source * dest + src ];
	};

	for ( DWORD i = 0; i < destination; i++ )
	{
		getOutput( 2, i ) = 0.0f;
	}

	// If downmixing, give environment a volume boost by normalizing channels
	if ( source > destination )
	{
		NormalizeOutputMatrix( levels, 0.5f );
	}
}

void SetOutputMatrixForAuxillary( UINT32 source, UINT32 destination, std::vector<float>& levels )
{
	auto getOutput = [&]( size_t src, size_t dest ) -> float& {
		return levels[ source * dest + src ];
	};

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
}