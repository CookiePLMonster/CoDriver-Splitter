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

HMODULE thisModule;
HMODULE hRealXAudio2;
HMODULE LoadRealXAudio2()
{
	if ( hRealXAudio2 != nullptr ) return hRealXAudio2;

	TCHAR systemPath[MAX_PATH];
	if ( GetSystemDirectory( systemPath, MAX_PATH ) == 0 ) return hRealXAudio2;

	// Obtain the name of this DLL and try to load a matching file
	TCHAR modulePath[MAX_PATH];
	if ( GetModuleFileName( thisModule, modulePath, MAX_PATH ) == 0 ) return hRealXAudio2;

	LPCTSTR fileName = PathFindFileName( modulePath );
	PathAppend( systemPath, fileName );

	return hRealXAudio2 = LoadLibrary( systemPath );
}

BOOL APIENTRY DllMain( HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID /*lpReserved*/
)
{

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		thisModule = hModule;
		break;
	}
	return TRUE;
}

