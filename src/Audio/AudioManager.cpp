// ============================================================
// File: AudioManager.cpp
// Responsibility: XAudio2 engine lifecycle, WAV loading, and
//                 looping BGM playback driven by EventManager events.
//
// Architecture:
//   States broadcast "bgm_play_overworld", "bgm_play_battle", or "bgm_stop".
//   AudioManager subscribes to these events in Initialize() and reacts by
//   calling PlayBGM() or StopBGM() internally.
//   States have zero knowledge of the audio subsystem.
//
// WAV support:
//   Reads standard RIFF/WAVE files with PCM (wFormatTag=1) or
//   IEEE-float (wFormatTag=3) sample data.  All audio chunks not named
//   "fmt " or "data" are skipped.  Word-alignment padding is respected.
//   WAVE_FORMAT_EXTENSIBLE (0xFFFE) files are not supported.
//
// Common mistakes:
//   1. Resetting mXAudio2 ComPtr before calling DestroyVoice() on child
//      voices — XAudio2 tears down the graph from under them, causing AV.
//   2. Calling PlayBGM with the same id twice — idempotent guard prevents
//      click/restart; always check mCurrentTrackId first.
//   3. Forgetting word-alignment padding in the RIFF chunk scanner —
//      chunks with odd byte counts have a silent pad byte after the data.
// ============================================================
#include "AudioManager.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include <fstream>
#include <sstream>
#include <cstring>

// ============================================================
// Local helpers
// ============================================================

// ------------------------------------------------------------
// LoadWavFile — minimal RIFF/WAVE parser.
//
// Reads the file entirely into memory, then walks the chunk list.
// "fmt " chunk fills wfxOut (PCM or IEEE float, mono or stereo).
// "data" chunk fills pcmOut.
// All other chunks (LIST, ID3, bext …) are silently skipped.
//
// Returns true only when both "fmt " and "data" chunks were found.
// ------------------------------------------------------------
static bool LoadWavFile(const std::string& path,
                        WAVEFORMATEX&       wfxOut,
                        std::vector<BYTE>&  pcmOut)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        LOG("[AudioManager] Could not open WAV: %s", path.c_str());
        return false;
    }

    // Read the whole file into memory once.  Avoids repeated seek/read
    // overhead and lets us use pointer arithmetic on the raw bytes.
    const std::streamsize fileSize = f.tellg();
    f.seekg(0);
    std::vector<BYTE> raw(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(raw.data()), fileSize);
    if (!f)
    {
        LOG("[AudioManager] Failed to read WAV: %s", path.c_str());
        return false;
    }

    const BYTE* p   = raw.data();
    const BYTE* end = p + raw.size();

    // Little-endian read helpers (WAV is always little-endian).
    auto r32 = [](const BYTE* b) -> UINT32 {
        return UINT32(b[0]) | (UINT32(b[1]) << 8) | (UINT32(b[2]) << 16) | (UINT32(b[3]) << 24);
    };
    auto r16 = [](const BYTE* b) -> UINT16 {
        return UINT16(b[0]) | (UINT16(b[1]) << 8);
    };

    // RIFF header: "RIFF" <fileSize-8> "WAVE"
    if (p + 12 > end || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
    {
        LOG("[AudioManager] Not a valid RIFF/WAVE file: %s", path.c_str());
        return false;
    }
    p += 12;

    bool hasFmt = false;
    bool hasData = false;

    while (p + 8 <= end)
    {
        const char* chunkId   = reinterpret_cast<const char*>(p);
        const UINT32 chunkSize = r32(p + 4);
        p += 8;

        // Guard against a malformed chunk that claims to extend past EOF.
        if (p + chunkSize > end) break;

        if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            // Minimum 16-byte PCM format block.  Extra bytes (e.g., cbSize
            // field in non-PCM formats) are intentionally ignored; only the
            // first 16 bytes are used to populate WAVEFORMATEX.
            wfxOut.wFormatTag      = r16(p);
            wfxOut.nChannels       = r16(p + 2);
            wfxOut.nSamplesPerSec  = r32(p + 4);
            wfxOut.nAvgBytesPerSec = r32(p + 8);
            wfxOut.nBlockAlign     = r16(p + 12);
            wfxOut.wBitsPerSample  = r16(p + 14);
            wfxOut.cbSize          = 0;   // XAudio2 does not use cbSize for PCM/float
            hasFmt = true;
        }
        else if (memcmp(chunkId, "data", 4) == 0)
        {
            // Copy the raw PCM samples into pcmOut.
            // The vector is sized exactly to the chunk and kept alive in
            // TrackData::pcmData — XAudio2 stores a pointer into this buffer.
            pcmOut.assign(p, p + chunkSize);
            hasData = true;
        }
        // Any other chunk (LIST, ID3, bext, smpl …) is silently skipped.

        p += chunkSize;

        // RIFF chunks are word-aligned: skip one pad byte for odd-sized chunks.
        if (chunkSize & 1) ++p;
    }

    if (!hasFmt)  LOG("[AudioManager] Missing fmt  chunk: %s", path.c_str());
    if (!hasData) LOG("[AudioManager] Missing data chunk: %s", path.c_str());
    return hasFmt && hasData;
}

// ============================================================
// Singleton
// ============================================================

AudioManager& AudioManager::Get()
{
    static AudioManager instance;
    return instance;
}

// ============================================================
// Initialize
// ============================================================

bool AudioManager::Initialize()
{
    // COM is required by XAudio2's internal CoCreateInstance call.
    // Track whether WE initialised it so Shutdown() can pair the call.
    const HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // S_OK          — COM initialised by us.
    // S_FALSE       — COM already initialised with the same model; refcount bumped.
    // RPC_E_CHANGED_MODE — already initialised with a different apartment model (e.g.,
    //                      single-threaded from WinMain).  Still usable; just don't
    //                      uninitialise on our side.
    mCoInitialized = SUCCEEDED(hrCom);

    // Create the XAudio2 engine.  XAUDIO2_DEFAULT_PROCESSOR lets the OS pick
    // the best audio processing thread; 0 flags disables the debug layer.
    HRESULT hr = XAudio2Create(mXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        LOG("[AudioManager] XAudio2Create failed (HRESULT 0x%08X). Audio disabled.", hr);
        return false;
    }

    // Mastering voice mixes all source voices to the default audio device
    // at the device's native channel count and sample rate.
    hr = mXAudio2->CreateMasteringVoice(&mMasterVoice);
    if (FAILED(hr))
    {
        LOG("[AudioManager] CreateMasteringVoice failed (0x%08X). Audio disabled.", hr);
        mXAudio2.Reset();
        return false;
    }

    // Preload all BGM tracks from the config file.
    // Voices are created once here and reused for every stop/play cycle.
    LoadBgmConfig("data/audio/bgm.json");

    // Subscribe to BGM control events.
    // Lambdas capture 'this' — safe because AudioManager is a singleton that
    // outlives all states.  Subscriptions are removed in Shutdown().
    mListenerPlayOverworld = EventManager::Get().Subscribe("bgm_play_overworld",
        [this](const EventData&) { PlayBGM("overworld"); });

    mListenerPlayBattle = EventManager::Get().Subscribe("bgm_play_battle",
        [this](const EventData&) { PlayBGM("battle"); });

    mListenerStop = EventManager::Get().Subscribe("bgm_stop",
        [this](const EventData&) { StopBGM(); });

    mInitialized = true;
    LOG("[AudioManager] Initialized. %zu BGM track(s) loaded.", mTracks.size());
    return true;
}

// ============================================================
// Shutdown
// ============================================================

void AudioManager::Shutdown()
{
    if (!mInitialized) return;

    // Remove event subscriptions FIRST — prevents events fired during shutdown
    // (e.g., from state destructors) from calling into a partially torn-down manager.
    EventManager::Get().Unsubscribe("bgm_play_overworld", mListenerPlayOverworld);
    EventManager::Get().Unsubscribe("bgm_play_battle",    mListenerPlayBattle);
    EventManager::Get().Unsubscribe("bgm_stop",           mListenerStop);

    // Stop and destroy all source voices.
    // Source voices MUST be destroyed before the mastering voice and the engine —
    // the engine graph flows from source → master → device; tearing it down in
    // reverse order prevents a data-race on the XAudio2 render thread.
    for (auto& [id, track] : mTracks)
    {
        if (track.voice)
        {
            track.voice->Stop(0);
            track.voice->FlushSourceBuffers();
            track.voice->DestroyVoice();
            track.voice = nullptr;
        }
    }
    mTracks.clear();
    mCurrentTrackId.clear();

    // Mastering voice must be destroyed before IXAudio2 is released.
    if (mMasterVoice)
    {
        mMasterVoice->DestroyVoice();
        mMasterVoice = nullptr;
    }

    // ComPtr destructor calls IXAudio2::Release() — the engine is freed here.
    mXAudio2.Reset();

    // Only uninitialise COM if we were the ones who initialised it.
    if (mCoInitialized)
    {
        CoUninitialize();
        mCoInitialized = false;
    }

    mInitialized = false;
    LOG("[AudioManager] Shutdown complete.");
}

// ============================================================
// LoadBgmConfig — parse data/audio/bgm.json
// ============================================================

void AudioManager::LoadBgmConfig(const std::string& configPath)
{
    std::ifstream f(configPath);
    if (!f.is_open())
    {
        LOG("[AudioManager] WARNING: bgm.json not found at '%s'. No BGM will play.",
            configPath.c_str());
        return;
    }

    // Read the entire file into a string for simple text scanning.
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Parse each {"id": "...", "path": "..."} object in the "tracks" array.
    // Scanning for "id" and "path" keys consecutively handles the expected layout.
    size_t pos = 0;
    while (true)
    {
        // Find the next "id" key.
        const size_t idKey = content.find("\"id\"", pos);
        if (idKey == std::string::npos) break;

        // Extract the quoted value after "id":
        size_t q1 = content.find('"', idKey + 4);
        if (q1 == std::string::npos) break;
        ++q1;
        const size_t q2 = content.find('"', q1);
        if (q2 == std::string::npos) break;
        const std::string trackId = content.substr(q1, q2 - q1);

        // Find the "path" key that immediately follows the id in the same object.
        const size_t pathKey = content.find("\"path\"", q2);
        if (pathKey == std::string::npos) break;

        size_t p1 = content.find('"', pathKey + 6);
        if (p1 == std::string::npos) break;
        ++p1;
        const size_t p2 = content.find('"', p1);
        if (p2 == std::string::npos) break;
        const std::string trackPath = content.substr(p1, p2 - p1);

        LoadTrack(trackId, trackPath);
        pos = p2 + 1;
    }
}

// ============================================================
// LoadTrack
// ============================================================

bool AudioManager::LoadTrack(const std::string& id, const std::string& path)
{
    TrackData track;

    if (!LoadWavFile(path, track.wfx, track.pcmData))
    {
        LOG("[AudioManager] Failed to load track '%s' from '%s'.",
            id.c_str(), path.c_str());
        return false;
    }

    // Create the source voice with the WAV's format descriptor.
    // The voice is created once and reused across all play/stop cycles —
    // voice creation is expensive; reuse avoids per-play allocation overhead.
    const HRESULT hr = mXAudio2->CreateSourceVoice(
        &track.voice,
        &track.wfx,
        0,                          // flags
        XAUDIO2_DEFAULT_FREQ_RATIO, // max pitch ratio (2.0 = one octave up)
        nullptr,                    // callback (none needed for BGM)
        nullptr,                    // send list (routes to master by default)
        nullptr                     // effect chain (none)
    );

    if (FAILED(hr))
    {
        LOG("[AudioManager] CreateSourceVoice failed for '%s' (0x%08X).",
            id.c_str(), hr);
        return false;
    }

    track.loaded = true;
    mTracks[id] = std::move(track);
    LOG("[AudioManager] Loaded BGM track '%s' (%zu bytes PCM).",
        id.c_str(), mTracks[id].pcmData.size());
    return true;
}

// ============================================================
// PlayBGM
// ============================================================

void AudioManager::PlayBGM(const std::string& trackId)
{
    // Idempotent: do nothing if the requested track is already playing.
    // Prevents an audible click or restart when the same state re-enters.
    if (mCurrentTrackId == trackId) return;

    // Stop the currently playing track (if any) before switching.
    StopBGM();

    auto it = mTracks.find(trackId);
    if (it == mTracks.end() || !it->second.loaded)
    {
        LOG("[AudioManager] PlayBGM: unknown or unloaded track '%s'.", trackId.c_str());
        return;
    }

    TrackData& track = it->second;

    // Build an XAUDIO2_BUFFER that loops the entire PCM data indefinitely.
    // LoopBegin=0, LoopLength=0 means "loop the whole buffer".
    // Flags=XAUDIO2_END_OF_STREAM signals the end of the stream to XAudio2
    // so it can make scheduling decisions; it does not stop a looping buffer.
    XAUDIO2_BUFFER buffer = {};
    buffer.Flags      = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(track.pcmData.size());
    buffer.pAudioData = track.pcmData.data();
    buffer.LoopCount  = XAUDIO2_LOOP_INFINITE;

    HRESULT hr = track.voice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr))
    {
        LOG("[AudioManager] SubmitSourceBuffer failed for '%s' (0x%08X).",
            trackId.c_str(), hr);
        return;
    }

    hr = track.voice->Start(0);
    if (FAILED(hr))
    {
        LOG("[AudioManager] IXAudio2SourceVoice::Start failed (0x%08X).", hr);
        return;
    }

    mCurrentTrackId = trackId;
    LOG("[AudioManager] Playing BGM: '%s'.", trackId.c_str());
}

// ============================================================
// StopBGM
// ============================================================

void AudioManager::StopBGM()
{
    if (mCurrentTrackId.empty()) return;

    auto it = mTracks.find(mCurrentTrackId);
    if (it != mTracks.end() && it->second.voice)
    {
        // Stop() halts playback immediately.  FlushSourceBuffers() discards
        // the looping buffer so SubmitSourceBuffer() starts from byte 0 next time.
        it->second.voice->Stop(0);
        it->second.voice->FlushSourceBuffers();
    }

    mCurrentTrackId.clear();
}
