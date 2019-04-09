#pragma once


// {6CE3D7FC-ED78-4256-BCAF-1088C153C9EA}
interface __declspec(uuid("6CE3D7FC-ED78-4256-BCAF-1088C153C9EA")) IMOXAudio2;

// Ignore the rest of this header if only the GUID definitions were requested
#ifndef GUID_DEFS_ONLY

#define MOXAUDIO2_VOICE_MULTIOUTPUT                 0x80000000    // Used in XAUDIO2_VOICE_DETAILS.CreationFlags to mark that this voice is a multi-output voice

__interface IMOXAudio2 : public IUnknown
{
	// Deliberately left empty, at the moment nothing is needed here
};

#endif