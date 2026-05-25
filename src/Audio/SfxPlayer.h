// ============================================================
// File: SfxPlayer.h
// Responsibility: One-shot sound-effect subsystem owned by AudioManager.
//
// Design summary:
//   - Loads named SFX "groups" from data/audio/sfx.json.  Each group
//     has 1..N WAV variants; PlaySfx() picks one at random (avoiding
//     the most-recently-played variant when the group has 2+ entries).
//   - Maintains two voice pools keyed by channel count -- 16 stereo
//     and 4 mono by default. All SFX in the asset library share
//     16-bit PCM 48kHz, so two channel-count buckets cover every file.
//   - Routes every SFX voice through a single submix voice.  One call
//     to SetMasterVolume() controls all SFX without touching BGM.
//
// Voice acquisition (called from PlaySfx):
//   1. Pick the pool matching the SFX's channel count.
//   2. Walk the pool from nextIdx forward, return the first voice with
//      BuffersQueued == 0 (idle).  Round-robin advance.
//   3. If every voice in the pool is busy, STEAL the oldest voice
//      (Stop + FlushSourceBuffers, then reuse).  Voice stealing is
//      the standard UI-SFX pattern -- queueing would feel laggy on
//      rapid menu navigation.
//
// EventManager integration:
//   AudioManager exposes PlaySfx() directly, AND subscribes to a
//   "sfx_play" broadcast whose payload is a const char* group id.
//   Most call sites use the direct API; the event path is provided
//   for systems-level code that prefers full decoupling.
//
// Lifetime:
//   Created in  -> AudioManager::Initialize() after the master voice exists.
//   Destroyed in -> AudioManager::Shutdown(), BEFORE the master voice and
//                   BEFORE the IXAudio2 engine is released.
//
// Common mistakes:
//   1. Submitting a buffer to a voice with the wrong format -> XAudio2
//      returns XAUDIO2_E_INVALID_CALL.  PickPool() guards by channel count.
//   2. Freeing the PCM byte vector while a voice still references it ->
//      audio-thread access violation.  Buffers live in mGroups for the
//      SfxPlayer's full lifetime.
//   3. Calling PlaySfx() from an XAudio2 callback thread -> nextIdx is
//      not guarded.  We only call from the game thread; document and
//      enforce by convention.
// ============================================================
#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

class SfxPlayer
{
public:
    SfxPlayer()  = default;
    ~SfxPlayer() = default;

    SfxPlayer(const SfxPlayer&)            = delete;
    SfxPlayer& operator=(const SfxPlayer&) = delete;

    // ------------------------------------------------------------
    // Initialize: build the submix voice, parse sfx.json, preload PCM
    //   buffers, and create the voice pools.
    // engine     - non-owning IXAudio2 pointer from AudioManager.
    // master     - non-owning master voice; submix routes here.
    // configPath - relative path to sfx.json (typically "data/audio/sfx.json").
    // Returns false on any catastrophic failure (engine null, submix
    // creation failed, no groups loaded).  Partial failures (one bad WAV)
    // log a warning but do not fail Initialize.
    // ------------------------------------------------------------
    bool Initialize(IXAudio2*               engine,
                    IXAudio2MasteringVoice* master,
                    const std::string&      configPath);

    // ------------------------------------------------------------
    // Shutdown: stop and DestroyVoice every pooled source voice, then
    //   the submix voice.  Engine is released by AudioManager afterwards.
    // ------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------
    // PlaySfx: play one variant of the named group.  No-op if the
    //   group is unknown or unloaded.  volumeMul applies on top of
    //   the per-group volume from sfx.json (1.0 = use config).
    // ------------------------------------------------------------
    void PlaySfx(const std::string& groupId, float volumeMul = 1.0f);

    // User gain for the whole SFX submix (0..1). It multiplies the
    // authored master baseline loaded from sfx.json.
    void  SetMasterVolume(float v);
    float GetMasterVolume() const { return mUserMasterVolume; }

private:
    // ------------------------------------------------------------
    // SfxGroup: one named entry in sfx.json with N variants.
    // pcmBuffers must outlive every voice created over its data
    // (XAudio2 retains the byte pointer for buffer playback).
    // ------------------------------------------------------------
    struct SfxGroup
    {
        WAVEFORMATEX                    format = {};
        std::vector<std::vector<BYTE>>  pcmBuffers;
        float                           volume = 1.0f;
        size_t                          lastPickedIndex = static_cast<size_t>(-1);
    };

    // ------------------------------------------------------------
    // VoicePool: round-robin pool of identical-format source voices.
    // Adding a new format means adding a new pool -- the asset library
    // currently uses two (stereo and mono) so two pools suffice.
    // ------------------------------------------------------------
    struct VoicePool
    {
        WAVEFORMATEX                       format = {};
        std::vector<IXAudio2SourceVoice*>  voices;   // raw -- DestroyVoice() in Shutdown
        size_t                             nextIdx = 0;
    };

    // Helpers (file-scope to keep the public surface minimal).
    bool                  LoadConfig(const std::string& path);
    bool                  LoadGroup (const std::string& id,
                                     const std::vector<std::string>& paths,
                                     float volume);
    bool                  CreatePool(VoicePool& pool,
                                     const WAVEFORMATEX& fmt,
                                     int count);
    IXAudio2SourceVoice*  AcquireVoice(VoicePool& pool);
    VoicePool*            PickPool(const WAVEFORMATEX& fmt);
    void                  ApplyMasterVolume();

    IXAudio2*                    mEngine = nullptr;   // borrowed from AudioManager
    IXAudio2SubmixVoice*         mSubmix = nullptr;   // owned

    std::unordered_map<std::string, SfxGroup> mGroups;

    VoicePool mStereoPool;
    VoicePool mMonoPool;

    int mStereoVoiceCount = 16;
    int mMonoVoiceCount   = 4;

    float mConfigMasterVolume = 1.0f;
    float mUserMasterVolume = 1.0f;

    bool mInitialized = false;
};
