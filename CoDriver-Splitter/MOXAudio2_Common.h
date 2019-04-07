#pragma once

#include <string>
#include <vector>

void FixupMasteringVoiceChannelMask( DWORD* pChannelmask );

void SetOutputMatrixForMain( UINT32 source, UINT32 destination, std::vector<float>& levels );
void SetOutputMatrixForAuxillary( UINT32 source, UINT32 destination, std::vector<float>& levels );

SIZE_T CalculateAuxBufferSize( const WAVEFORMATEX* pSourceFormat );
DWORD PopCount( DWORD mask );

class AuxillaryVoiceRingBuffer
{
public:
	const BYTE*		CopyToRingBuffer( const BYTE* bytes, UINT32 size );

	AuxillaryVoiceRingBuffer(SIZE_T size);
	~AuxillaryVoiceRingBuffer();

private:
	BYTE*	Reset();

	CRITICAL_SECTION	m_mutex;
	BYTE*				m_buffer = nullptr;
	BYTE*				m_cursor = nullptr;
	const SIZE_T		m_bufferSize;
};