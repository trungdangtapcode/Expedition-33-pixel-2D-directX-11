// ============================================================
// File: MediaLoader.h
// Responsibility: Decode compressed audio files (MP3, WMA, AAC, …)
//                 into raw PCM buffers using Windows Media Foundation.
//
// Why Media Foundation:
//   XAudio2 accepts only raw PCM (or ADPCM) buffers.  WAV files are
//   already raw PCM, but compressed formats like MP3 need a codec.
//   Media Foundation is a Windows-native API that ships with every
//   Windows 7+ install — zero third-party dependencies.
//
// Usage:
//   WAVEFORMATEX wfx{};
//   std::vector<BYTE> pcm;
//   if (MediaLoader::LoadFile("assets/sound/OST/track.mp3", wfx, pcm))
//       // feed wfx + pcm to IXAudio2SourceVoice
//
// Supported formats (anything the OS has a codec for):
//   .mp3, .wma, .aac, .m4a, .flac, .ogg (Win10+), .wav
//   The output is always 16-bit PCM at the source sample rate.
//
// Lifetime:
//   MFStartup / MFShutdown are ref-counted by the OS.  Each LoadFile
//   call does its own Startup/Shutdown pair so the caller never needs
//   to manage MF lifetime globally.
//
// Owned resources:
//   All COM pointers are released before the function returns.
//   The only output is the filled wfxOut + pcmOut vectors.
//
// Common mistakes:
//   1. Forgetting to set MF_MT_SUBTYPE to MFAudioFormat_PCM on the
//      output type — the reader returns compressed samples instead.
//   2. Releasing the IMFSample before copying its buffer — the data
//      pointer becomes dangling.
//   3. Not checking MF_SOURCE_READERF_ENDOFSTREAM — infinite loop.
// ============================================================
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace MediaLoader
{
    // ------------------------------------------------------------
    // LoadFile: decode any Media-Foundation-supported audio file
    //   into 16-bit PCM samples.
    //
    // Parameters:
    //   path   — file path (narrow string, converted to wide internally)
    //   wfxOut — filled with the decoded PCM format descriptor
    //   pcmOut — filled with the raw PCM sample bytes
    //
    // Returns:
    //   true  — file decoded successfully
    //   false — file not found, unsupported codec, or MF error (logged)
    // ------------------------------------------------------------
    bool LoadFile(const std::string& path,
                  WAVEFORMATEX&      wfxOut,
                  std::vector<BYTE>& pcmOut);
}
