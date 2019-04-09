#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0601 // Opt for lowest supported version here (Win7)
#define _WIN32_WINNT 0x0601
#include <Windows.h>
#include <VersionHelpers.h>
#include <cassert>
#include <Shlwapi.h>

#include "MOXAudio2_Hooks.h"
#include "Detours/detours.h"

#pragma comment(lib, "shlwapi.lib")

#ifdef _WIN64
#pragma comment(lib, "Detours_x64.lib")
#else
#pragma comment(lib, "Detours.lib")
#endif

// Used by XAudio2.8 and XAudio2.9
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

// Used by XAudio2.7
HMODULE LoadRealLegacyXAudio2( bool debug )
{
	if ( hRealXAudio2 != nullptr ) return hRealXAudio2;

	TCHAR dllPath[MAX_PATH];
	if ( GetSystemDirectory( dllPath, MAX_PATH ) == 0 ) return hRealXAudio2;

	PathAppend( dllPath, debug ? TEXT("xaudioD2_7.dll") : TEXT("xaudio2_7.dll") );
	return hRealXAudio2 = LoadLibrary( dllPath );
}

auto* OrgCoCreateInstance = CoCreateInstance;
HRESULT WINAPI CoCreateInstance_Hook(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
	auto hr = CreateLegacyXAudio2( rclsid, riid, ppv );
	if ( hr )
	{
		return *hr;
	}
	return OrgCoCreateInstance( rclsid, pUnkOuter, dwClsContext, riid, ppv );
}


BOOL APIENTRY DllMain( HMODULE /*hModule*/,
	DWORD  ul_reason_for_call,
	LPVOID /*lpReserved*/
)
{
	if ( ul_reason_for_call != DLL_PROCESS_ATTACH && ul_reason_for_call != DLL_PROCESS_DETACH )
	{
		return TRUE;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	if ( ul_reason_for_call == DLL_PROCESS_ATTACH )
	{
		DetourAttach( &(PVOID&)OrgCoCreateInstance, CoCreateInstance_Hook );
	}
	else
	{
		DetourDetach( &(PVOID&)OrgCoCreateInstance, CoCreateInstance_Hook );
	}

	DetourTransactionCommit();

	return TRUE;
}

