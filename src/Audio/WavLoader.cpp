// ============================================================
// File: WavLoader.cpp
// Responsibility: Implementation of the shared RIFF/WAVE parser.
//                 Lifted out of AudioManager.cpp so the SFX subsystem
//                 can reuse the exact same chunk handling.
// ============================================================
#include "WavLoader.h"
#include "../Utils/Log.h"

#include <fstream>
#include <vector>
#include <cstring>

namespace WavLoader
{

bool LoadFile(const std::string& path,
              WAVEFORMATEX&      wfxOut,
              std::vector<BYTE>& pcmOut)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        LOG("[WavLoader] Could not open WAV: %s", path.c_str());
        return false;
    }

    // Read the whole file once.  Avoids repeated seek/read overhead and
    // lets us walk the chunk list with simple pointer arithmetic.
    const std::streamsize fileSize = f.tellg();
    f.seekg(0);
    std::vector<BYTE> raw(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(raw.data()), fileSize);
    if (!f)
    {
        LOG("[WavLoader] Failed to read WAV: %s", path.c_str());
        return false;
    }

    const BYTE* p   = raw.data();
    const BYTE* end = p + raw.size();

    // Little-endian helpers (RIFF/WAVE is always little-endian).
    auto r32 = [](const BYTE* b) -> UINT32 {
        return UINT32(b[0])
             | (UINT32(b[1]) << 8)
             | (UINT32(b[2]) << 16)
             | (UINT32(b[3]) << 24);
    };
    auto r16 = [](const BYTE* b) -> UINT16 {
        return UINT16(b[0]) | (UINT16(b[1]) << 8);
    };

    // RIFF header: "RIFF" <fileSize-8> "WAVE"
    if (p + 12 > end || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
    {
        LOG("[WavLoader] Not a valid RIFF/WAVE file: %s", path.c_str());
        return false;
    }
    p += 12;

    bool hasFmt  = false;
    bool hasData = false;

    while (p + 8 <= end)
    {
        const char*  chunkId   = reinterpret_cast<const char*>(p);
        const UINT32 chunkSize = r32(p + 4);
        p += 8;

        // Guard against a malformed chunk that claims to extend past EOF.
        if (p + chunkSize > end) break;

        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            // Minimum 16-byte PCM format block.  Extra bytes (cbSize and
            // beyond, used by non-PCM formats) are ignored on purpose --
            // we only declare a plain WAVEFORMATEX.
            wfxOut.wFormatTag      = r16(p);
            wfxOut.nChannels       = r16(p + 2);
            wfxOut.nSamplesPerSec  = r32(p + 4);
            wfxOut.nAvgBytesPerSec = r32(p + 8);
            wfxOut.nBlockAlign     = r16(p + 12);
            wfxOut.wBitsPerSample  = r16(p + 14);
            wfxOut.cbSize          = 0;
            hasFmt = true;
        }
        else if (memcmp(chunkId, "data", 4) == 0)
        {
            // Copy the raw PCM samples into pcmOut.  The vector must be
            // kept alive by the caller for as long as any IXAudio2 voice
            // references it -- XAudio2 stores a pointer into the buffer.
            pcmOut.assign(p, p + chunkSize);
            hasData = true;
        }
        // All other chunks (LIST, ID3, bext, smpl, …) are intentionally
        // ignored.  The walker still advances by chunkSize so they do
        // not break the parser.

        p += chunkSize;

        // RIFF chunks are word-aligned: skip one pad byte for odd sizes.
        if (chunkSize & 1) ++p;
    }

    if (!hasFmt)  LOG("[WavLoader] Missing 'fmt ' chunk: %s", path.c_str());
    if (!hasData) LOG("[WavLoader] Missing 'data' chunk: %s", path.c_str());
    return hasFmt && hasData;
}

} // namespace WavLoader
