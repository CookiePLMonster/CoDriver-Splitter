#pragma once

#include <optional>

std::optional<HRESULT> CreateLegacyXAudio2(REFCLSID rclsid, REFIID riid, LPVOID *ppv);

HMODULE LoadRealXAudio2();
HMODULE LoadRealLegacyXAudio2( bool debug );