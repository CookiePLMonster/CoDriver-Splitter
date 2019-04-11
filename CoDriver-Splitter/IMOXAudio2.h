#pragma once


// {6CE3D7FC-ED78-4256-BCAF-1088C153C9EA}
interface __declspec(uuid("6CE3D7FC-ED78-4256-BCAF-1088C153C9EA")) IMOXAudio2;

// Ignore the rest of this header if only the GUID definitions were requested
#ifndef GUID_DEFS_ONLY

#define MOXAUDIO2_VOICE_MULTIOUTPUT                 0x80000000    // Used in XAUDIO2_VOICE_DETAILS.CreationFlags to mark that this voice is a multi-output voice

__interface IMOXAudio2 : public IUnknown
{
	// Obtains underlying IXAudio2 devices. This function increases the reference count of returned objects.
	HRESULT WINAPI GetInternalObjects( IXAudio2** mainDevice, IXAudio2** auxDevice );

	// Given an IXAudio2Voice, obtains underlying IXAudio2Voice's. If a source voice is not from MOXAudio2 device, function reports failure.
	HRESULT WINAPI GetInternalVoices( IXAudio2Voice* srcVoice, IXAudio2Voice** outMainVoice, IXAudio2Voice** outAuxVoice );
};

#endif