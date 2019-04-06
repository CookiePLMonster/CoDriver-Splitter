#pragma once

#include <string>
#include <vector>

std::wstring GetCommunicationsDeviceString();
void FixupMasteringVoiceChannelMask( DWORD* pChannelmask );

void SetOutputMatrixForMain( UINT32 source, UINT32 destination, std::vector<float>& levels );
void SetOutputMatrixForAuxillary( UINT32 source, UINT32 destination, std::vector<float>& levels );

DWORD PopCount( DWORD mask );