#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0601 // Opt for lowest supported version here (Win7)
#define _WIN32_WINNT 0x0601
#include <Windows.h>

#include <mmreg.h>

#include "MOXAudio2_Common.h"

#include <wrl.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>

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

SIZE_T CalculateAuxBufferSize( const WAVEFORMATEX* pSourceFormat )
{
	// Wwise seems to submit samples to XAudio2 in chunks of 1024
	// To be safe, we'll make a buffer big enough to store a full second of audio and align it up to 1024
	SIZE_T RingBufferSize = pSourceFormat->nAvgBytesPerSec;
	if ( RingBufferSize == 0 )
	{
		RingBufferSize = pSourceFormat->nSamplesPerSec * pSourceFormat->nBlockAlign;
	}
	RingBufferSize = (RingBufferSize + 1023) & ~(1023);
	return RingBufferSize;
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

const BYTE* AuxillaryVoiceRingBuffer::CopyToRingBuffer( const BYTE* bytes, UINT32 size )
{
	BYTE* space;

	EnterCriticalSection( &m_mutex );
	space = m_cursor;
	if ( space + size > m_buffer + m_bufferSize )
	{
		space = Reset();
	}
	m_cursor += size;
	LeaveCriticalSection( &m_mutex );

	return static_cast<BYTE*>(memcpy( space, bytes, size ));
}

AuxillaryVoiceRingBuffer::AuxillaryVoiceRingBuffer( SIZE_T size )
	: m_bufferSize(size)
{
	InitializeCriticalSection(&m_mutex);
	m_buffer = new BYTE[m_bufferSize];

	Reset();
}

AuxillaryVoiceRingBuffer::~AuxillaryVoiceRingBuffer()
{
	delete[] m_buffer;
	DeleteCriticalSection(&m_mutex);
}

BYTE* AuxillaryVoiceRingBuffer::Reset()
{
	return m_cursor = m_buffer;
}
