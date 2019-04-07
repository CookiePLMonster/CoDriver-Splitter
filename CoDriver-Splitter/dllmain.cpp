#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0601 // Opt for lowest supported version here (Win7)
#define _WIN32_WINNT 0x0601
#include <Windows.h>
#include <VersionHelpers.h>
#include <cassert>
#include <Shlwapi.h>

#include "MOXAudio2_Hooks.h"

#pragma comment(lib, "shlwapi.lib")

HMODULE hRealXAudio2;
HMODULE LoadRealXAudio2()
{
	if ( hRealXAudio2 != nullptr ) return hRealXAudio2;

	TCHAR systemPath[MAX_PATH];
	if ( GetSystemDirectory( systemPath, MAX_PATH ) == 0 ) return hRealXAudio2;

	// First try to load XAudio 2.9, no matter what DLL this is
	// On Windows 8/8.1, it'll fail so fall back to XAudio 2.8
	TCHAR dllPath[MAX_PATH];
	PathCombine( dllPath, systemPath, TEXT("xaudio2_9.dll") );
	hRealXAudio2 = LoadLibrary( dllPath );
	if ( hRealXAudio2 != nullptr ) return hRealXAudio2;

	PathCombine( dllPath, systemPath, TEXT("xaudio2_8.dll") );
	return hRealXAudio2 = LoadLibrary( dllPath );
}

BOOL APIENTRY DllMain( HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID /*lpReserved*/
)
{
	return TRUE;
}

