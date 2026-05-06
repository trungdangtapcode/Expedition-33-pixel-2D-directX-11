// ============================================================
// File: MediaLoader.cpp
// Responsibility: Decode compressed audio (MP3, WMA, AAC, FLAC, …)
//                 to 16-bit PCM using Windows Media Foundation.
//
// Architecture:
//   1. MFStartup() — initialise the MF runtime (ref-counted; safe to
//      call multiple times).
//   2. MFCreateSourceReaderFromURL — open the file and auto-select
//      the best installed codec for the container format.
//   3. Set the output media type to uncompressed 16-bit PCM so the
//      Source Reader performs the decode internally.
//   4. Read samples in a loop until MF_SOURCE_READERF_ENDOFSTREAM,
//      copying each IMFMediaBuffer into the output vector.
//   5. Extract the final WAVEFORMATEX from the negotiated output type
//      so the caller can create an IXAudio2SourceVoice with the
//      correct sample rate / channel count.
//   6. MFShutdown() — release the MF runtime ref.
//
// Lifetime:
//   Every COM pointer is released before the function returns.
//   The output vectors (wfxOut, pcmOut) are the only side effects.
//
// Common mistakes:
//   1. Skipping MFStartup → MFCreateSourceReaderFromURL returns
//      MF_E_PLATFORM_NOT_INITIALIZED.
//   2. Not locking the IMFMediaBuffer before reading its data pointer
//      → access violation on the MF worker thread.
//   3. Passing a narrow-string path to MFCreateSourceReaderFromURL
//      → it requires a wide (LPCWSTR) path.
// ============================================================
#include "MediaLoader.h"
#include "../Utils/Log.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

bool MediaLoader::LoadFile(const std::string& path,
                           WAVEFORMATEX&      wfxOut,
                           std::vector<BYTE>& pcmOut)
{
    pcmOut.clear();

    // Convert narrow path to wide — MF only accepts LPCWSTR.
    // All asset paths in this project are 7-bit ASCII.
    std::wstring widePath(path.begin(), path.end());

    // Initialise the Media Foundation runtime.  Ref-counted by the OS,
    // so nested calls are safe — each MFStartup must pair with MFShutdown.
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        LOG("[MediaLoader] MFStartup failed (0x%08X).", hr);
        return false;
    }

    // Create a Source Reader that auto-detects the container and codec.
    // MFCreateSourceReaderFromURL handles MP3, WMA, AAC, FLAC, OGG (Win10+),
    // and even WAV — any format the OS has a codec for.
    IMFSourceReader* reader = nullptr;
    hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &reader);
    if (FAILED(hr))
    {
        LOG("[MediaLoader] Cannot open '%s' (0x%08X).", path.c_str(), hr);
        MFShutdown();
        return false;
    }

    // Tell the Source Reader to decode to uncompressed 16-bit PCM.
    // We create a partial media type with only the major type and subtype
    // set; MF negotiates the rest (sample rate, channels) from the source.
    IMFMediaType* outputType = nullptr;
    MFCreateMediaType(&outputType);
    outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outputType->SetGUID(MF_MT_SUBTYPE,    MFAudioFormat_PCM);
    outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);

    hr = reader->SetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
        nullptr, outputType);
    outputType->Release();

    if (FAILED(hr))
    {
        LOG("[MediaLoader] SetCurrentMediaType failed for '%s' (0x%08X).",
            path.c_str(), hr);
        reader->Release();
        MFShutdown();
        return false;
    }

    // Read back the fully negotiated output type to extract the final
    // WAVEFORMATEX (sample rate, channel count, block align, etc.).
    IMFMediaType* actualType = nullptr;
    hr = reader->GetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
        &actualType);
    if (FAILED(hr))
    {
        LOG("[MediaLoader] GetCurrentMediaType failed (0x%08X).", hr);
        reader->Release();
        MFShutdown();
        return false;
    }

    // MFCreateWaveFormatExFromMFMediaType allocates a WAVEFORMATEX on the
    // heap.  We copy the first sizeof(WAVEFORMATEX) bytes into the caller's
    // struct and free the allocation.
    WAVEFORMATEX* pWfx   = nullptr;
    UINT32        wfxLen = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(actualType, &pWfx, &wfxLen, 0);
    actualType->Release();

    if (FAILED(hr) || !pWfx)
    {
        LOG("[MediaLoader] MFCreateWaveFormatExFromMFMediaType failed (0x%08X).", hr);
        reader->Release();
        MFShutdown();
        return false;
    }
    memcpy(&wfxOut, pWfx, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWfx);

    // Read decoded PCM samples until end-of-stream.
    // Each ReadSample call returns one IMFSample containing one or more
    // IMFMediaBuffers of raw PCM data.
    while (true)
    {
        DWORD      flags  = 0;
        IMFSample* sample = nullptr;

        hr = reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            0,          // no control flags
            nullptr,    // actual stream index (don't care)
            &flags,
            nullptr,    // timestamp (don't care for full decode)
            &sample);

        if (FAILED(hr)) break;

        // End-of-stream — all samples have been decoded.
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

        if (!sample) continue;

        // Each sample may contain multiple buffers; ConvertToContiguousBuffer
        // merges them into a single buffer for simpler copy logic.
        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (SUCCEEDED(hr) && buffer)
        {
            BYTE*  data   = nullptr;
            DWORD  length = 0;

            // Lock the buffer to get a raw pointer to the PCM bytes.
            // The lock must be released before the buffer is freed.
            hr = buffer->Lock(&data, nullptr, &length);
            if (SUCCEEDED(hr) && data && length > 0)
            {
                pcmOut.insert(pcmOut.end(), data, data + length);
                buffer->Unlock();
            }
            buffer->Release();
        }
        sample->Release();
    }

    reader->Release();
    MFShutdown();

    if (pcmOut.empty())
    {
        LOG("[MediaLoader] Decoded 0 bytes from '%s' — file may be empty or corrupt.",
            path.c_str());
        return false;
    }

    LOG("[MediaLoader] Decoded '%s': %u Hz, %u ch, %zu bytes PCM.",
        path.c_str(), wfxOut.nSamplesPerSec, wfxOut.nChannels, pcmOut.size());
    return true;
}
