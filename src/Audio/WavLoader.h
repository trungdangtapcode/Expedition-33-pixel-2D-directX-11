// ============================================================
// File: WavLoader.h
// Responsibility: Minimal RIFF/WAVE parser shared by AudioManager
//                 (BGM tracks) and SfxPlayer (one-shot SFX groups).
//
// Why a shared file:
//   Both BGM and SFX decode the same chunk layout into the same
//   {WAVEFORMATEX, std::vector<BYTE>} pair before feeding it to
//   IXAudio2SourceVoice.  Duplicating the parser invites a fix-here-
//   forget-there bug; a single helper guarantees both code paths
//   handle malformed input the same way.
//
// Supported subset:
//   - PCM (wFormatTag = 1)        — every game asset uses this
//   - IEEE float (wFormatTag = 3) — supported but not required today
//   - WAVE_FORMAT_EXTENSIBLE (0xFFFE) is NOT supported.  Re-export
//     the WAV from the audio tool to a plain PCM container.
//   - Mono and stereo at any sample rate / bit depth.
//
// Common mistakes this helper guards against:
//   1. RIFF chunks are word-aligned: a chunk with an odd byteCount
//      is followed by a silent pad byte.  Forgetting the pad shifts
//      every later chunk by one byte and the "data" chunk is missed.
//   2. The "fmt " chunk may legally be larger than 16 bytes for
//      non-PCM formats.  We read only the first 16 bytes (enough for
//      WAVEFORMATEX in the PCM/float cases we accept).
//   3. The XAudio2 source voice retains a pointer into the PCM byte
//      buffer for the entire playback.  The caller must keep the
//      vector alive until the voice is destroyed -- this header only
//      loads, it does not own the buffer.
// ============================================================
#pragma once
#include <windows.h>     // WAVEFORMATEX
#include <mmreg.h>       // WAVE_FORMAT_PCM constants
#include <string>
#include <vector>

namespace WavLoader
{
    // Returns true only when both the "fmt " and "data" chunks were
    // found and successfully parsed.  On failure: logs through the
    // project's LOG() macro and leaves wfxOut / pcmOut in an
    // unspecified state -- callers should not use them.
    bool LoadFile(const std::string& path,
                  WAVEFORMATEX&      wfxOut,
                  std::vector<BYTE>& pcmOut);
}
