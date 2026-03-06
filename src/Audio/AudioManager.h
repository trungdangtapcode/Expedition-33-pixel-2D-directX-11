// ============================================================
// File: AudioManager.h
// Responsibility: Global audio manager — owns the XAudio2 engine,
//                 preloads BGM tracks from data/audio/bgm.json, and
//                 handles looping BGM playback with internal state tracking.
//
// Pattern: Meyers' Singleton + Observer (EventManager subscription).
//   States NEVER call AudioManager directly — they broadcast named events:
//     "bgm_play_overworld"  ->  PlayBGM("overworld")
//     "bgm_play_battle"     ->  PlayBGM("battle")
//     "bgm_stop"            ->  StopBGM()
//   This keeps every game state fully decoupled from the audio subsystem.
//
// BGM internal state machine:
//   PlayBGM(id) is IDEMPOTENT — if 'id' is already the current track, it is
//   a no-op.  No restarts, no clicks.  Switching tracks: stop current voice,
//   flush buffer, resubmit loop buffer for the new track, start.
//
// Track configuration  (data/audio/bgm.json):
//   { "tracks": [ {"id":"overworld","path":"..."}, {"id":"battle","path":"..."} ] }
//   Paths are relative to the workspace root (same convention as all other assets).
//
// Owned resources:
//   IXAudio2               — XAudio2 engine (ComPtr, auto-released on Reset())
//   IXAudio2MasteringVoice — master output voice (raw ptr, DestroyVoice() in Shutdown())
//   IXAudio2SourceVoice    — one per BGM track  (raw ptr, DestroyVoice() in Shutdown())
//   std::vector<BYTE>      — PCM buffer per track (must outlive the voice)
//
// Lifetime:
//   Initialize() — called from GameApp::Initialize(), before the first state push.
//   Shutdown()   — called from GameApp::~GameApp(), after the state stack is cleared.
//
// Common mistakes:
//   1. Calling IXAudio2::Release() (via ComPtr reset) before DestroyVoice() on
//      all child voices — the engine tears down the audio graph from underneath them.
//   2. Freeing the PCM vector while the source voice is still playing — XAudio2
//      references the buffer pointer; the CPU crash happens on the XAudio2 thread.
//   3. Creating a new IXAudio2SourceVoice on every PlayBGM() call — voices are
//      reusable; always stop/flush/resubmit the existing voice instead.
//   4. Calling CoUninitialize() when COM was initialised by another subsystem
//      (e.g., DirectX 11) — only uninitialise if AudioManager itself called
//      CoInitializeEx successfully (tracked by mCoInitialized).
// ============================================================
#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

class AudioManager
{
public:
    // ------------------------------------------------------------
    // Singleton accessor — Meyers' pattern, thread-safe from C++11.
    // ------------------------------------------------------------
    static AudioManager& Get();

    // Non-copyable / non-movable (singleton invariant).
    AudioManager(const AudioManager&)            = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // ------------------------------------------------------------
    // Initialize: create the XAudio2 engine, load bgm.json, and
    //   preload all BGM tracks.  Subscribes to EventManager events.
    //   Must be called before any state broadcasts a BGM event.
    // Returns false if XAudio2 creation fails (audio will be silent).
    // ------------------------------------------------------------
    bool Initialize();

    // ------------------------------------------------------------
    // Shutdown: stop all voices, unsubscribe events, destroy engine.
    //   Safe to call even if Initialize() was never called or failed.
    // ------------------------------------------------------------
    void Shutdown();

    // Convenience query — used by states (optional) to skip broadcasts
    // when audio is unavailable (e.g., headless test environment).
    bool IsInitialized() const { return mInitialized; }

private:
    AudioManager()  = default;
    ~AudioManager() = default;

    // ---------------------------------------------------------------
    // Per-track data.
    //
    // The source voice is created ONCE at load time and reused for every
    // stop/play cycle — creating a new voice per play would accumulate
    // unused voices and eventually exhaust XAudio2's voice pool.
    //
    // pcmData must remain alive for the entire voice lifetime.  It is
    // stored here (not as a temporary) to satisfy that invariant.
    // ---------------------------------------------------------------
    struct TrackData
    {
        IXAudio2SourceVoice* voice   = nullptr;  // raw ptr; DestroyVoice() in Shutdown
        std::vector<BYTE>    pcmData;             // PCM samples; must outlive voice
        WAVEFORMATEX         wfx     = {};        // format descriptor passed to voice
        bool                 loaded  = false;
    };

    // ---------------------------------------------------------------
    // BGM playback — private.
    //   States interact through EventManager, not these methods directly.
    // ---------------------------------------------------------------

    // Play the track with the given id.  No-op if already playing.
    // Stops and flushes the current track before switching.
    void PlayBGM(const std::string& trackId);

    // Stop the current track immediately.
    void StopBGM();

    // ---------------------------------------------------------------
    // Asset loading helpers.
    // ---------------------------------------------------------------

    // Parse data/audio/bgm.json and call LoadTrack() for each entry.
    void LoadBgmConfig(const std::string& configPath);

    // Load a WAV file at 'path' and create an IXAudio2SourceVoice for it.
    // Returns true on success; logs a warning and returns false on failure.
    bool LoadTrack(const std::string& id, const std::string& path);

    // ---------------------------------------------------------------
    // Members.
    // ---------------------------------------------------------------

    // XAudio2 engine — ComPtr ensures Release() is called before the
    // destructor returns, even if exceptions occur (not used here, but
    // good practice for integration with exception-enabled code).
    ComPtr<IXAudio2>        mXAudio2;

    // Master voice mixes all source voices down to the audio device.
    // Must be destroyed AFTER all source voices and BEFORE mXAudio2.Reset().
    IXAudio2MasteringVoice* mMasterVoice = nullptr;

    // Keyed by track id string ("overworld", "battle", …).
    std::unordered_map<std::string, TrackData> mTracks;

    // ID of the track currently playing, or "" when stopped.
    // Used by PlayBGM() to implement the idempotent no-op check.
    std::string mCurrentTrackId;

    // EventManager subscription IDs — stored so Shutdown() can clean up.
    // Dangling subscriptions would fire into a destroyed AudioManager.
    int mListenerPlayOverworld = -1;
    int mListenerPlayBattle    = -1;
    int mListenerStop          = -1;

    bool mInitialized   = false;

    // True only if THIS call to Initialize() succeeded in CoInitializeEx.
    // We must not CoUninitialize() COM that another subsystem initialised.
    bool mCoInitialized = false;
};
