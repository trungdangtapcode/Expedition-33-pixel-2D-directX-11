// ============================================================
// File: AudioManager.cpp
// Responsibility: XAudio2 engine lifecycle, audio loading, and
//                 data-driven BGM playback driven by EventManager events.
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
//      voices - XAudio2 tears down the graph from under them, causing AV.
//   2. Calling PlayBGM with the same looping id twice - idempotent guard
//      prevents click/restart; one-shot tracks replay only after finishing.
//   3. Forgetting word-alignment padding in the RIFF chunk scanner -
//      chunks with odd byte counts have a silent pad byte after the data.
// ============================================================
#include "AudioManager.h"
#include "WavLoader.h"
#include "MediaLoader.h"
#include "../Events/EventManager.h"
#include "../Battle/BattleEvents.h"
#include "../Systems/SettingsManager.h"
#include "../Utils/Log.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>

namespace
{
    float ClampVolume(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    size_t FindMatchingBrace(const std::string& text, size_t openBrace)
    {
        bool inString = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = openBrace; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (escaped)
            {
                escaped = false;
                continue;
            }

            if (ch == '\\' && inString)
            {
                escaped = true;
                continue;
            }

            if (ch == '"')
            {
                inString = !inString;
                continue;
            }

            if (inString) continue;

            if (ch == '{')
            {
                ++depth;
            }
            else if (ch == '}')
            {
                --depth;
                if (depth == 0) return i;
            }
        }

        return std::string::npos;
    }

    size_t FindMatchingBracket(const std::string& text, size_t openBracket)
    {
        bool inString = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = openBracket; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (escaped)
            {
                escaped = false;
                continue;
            }

            if (ch == '\\' && inString)
            {
                escaped = true;
                continue;
            }

            if (ch == '"')
            {
                inString = !inString;
                continue;
            }

            if (inString) continue;

            if (ch == '[')
            {
                ++depth;
            }
            else if (ch == ']')
            {
                --depth;
                if (depth == 0) return i;
            }
        }

        return std::string::npos;
    }

    bool ExtractStringValue(const std::string& objectText,
                            const std::string& key,
                            std::string& outValue)
    {
        const std::string quotedKey = "\"" + key + "\"";
        const size_t keyPos = objectText.find(quotedKey);
        if (keyPos == std::string::npos) return false;

        const size_t colonPos = objectText.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string::npos) return false;

        const size_t valueStart = objectText.find('"', colonPos + 1);
        if (valueStart == std::string::npos) return false;

        const size_t valueEnd = objectText.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) return false;

        outValue = objectText.substr(valueStart + 1, valueEnd - valueStart - 1);
        return true;
    }

    bool ExtractBoolValueOrDefault(const std::string& objectText,
                                   const std::string& key,
                                   bool defaultValue)
    {
        const std::string quotedKey = "\"" + key + "\"";
        const size_t keyPos = objectText.find(quotedKey);
        if (keyPos == std::string::npos) return defaultValue;

        const size_t colonPos = objectText.find(':', keyPos + quotedKey.size());
        if (colonPos == std::string::npos) return defaultValue;

        size_t valuePos = colonPos + 1;
        while (valuePos < objectText.size() &&
               std::isspace(static_cast<unsigned char>(objectText[valuePos])))
        {
            ++valuePos;
        }

        if (objectText.compare(valuePos, 4, "true") == 0) return true;
        if (objectText.compare(valuePos, 5, "false") == 0) return false;
        return defaultValue;
    }
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
    // S_OK          - COM initialised by us.
    // S_FALSE       - COM already initialised with the same model; refcount bumped.
    // RPC_E_CHANGED_MODE - already initialised with a different apartment model (e.g.,
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

    if (!CreateSubmixBus(&mBgmSubmix, "BGM"))
    {
        Shutdown();
        return false;
    }

    if (!CreateSubmixBus(&mVoiceSubmix, "Voice"))
    {
        Shutdown();
        return false;
    }

    SetBgmMasterVolume(SettingsManager::Get().GetBgmVolume());
    SetVoiceMasterVolume(SettingsManager::Get().GetVoiceVolume());

    // Preload all BGM tracks from the config file.
    // Voices are created once here and reused for every stop/play cycle.
    LoadBgmConfig("data/audio/bgm.json");

    // Subscribe to BGM control events.
    // Lambdas capture 'this' - safe because AudioManager is a singleton that
    // outlives all states.  Subscriptions are removed in Shutdown().
    mListenerPlayOverworld = EventManager::Get().Subscribe("bgm_play_overworld",
        [this](const EventData&) { PlayBGM("overworld"); });

    mListenerPlayBattle = EventManager::Get().Subscribe("bgm_play_battle",
        [this](const EventData&) { PlayBGM("battle"); });

    mListenerStop = EventManager::Get().Subscribe("bgm_stop",
        [this](const EventData&) { StopBGM(); });

    // Generic BGM event: payload is a const char* track id from bgm.json.
    // This lets any system play an arbitrary track without a dedicated
    // per-track event subscription.  The fixed events above are kept for
    // backward compatibility.
    mListenerBgmPlay = EventManager::Get().Subscribe("bgm_play",
        [this](const EventData& e) {
            if (e.payload)
                PlayBGM(static_cast<const char*>(e.payload));
        });

    // Build the SFX subsystem on top of the same engine + master voice.
    // Failure here is non-fatal -- BGM still works and PlaySfx() becomes
    // a no-op until the cause is fixed.
    mSfx.Initialize(mXAudio2.Get(), mMasterVoice, "data/audio/sfx.json");
    SetSfxMasterVolume(SettingsManager::Get().GetSfxVolume());

    // SFX event bus.  Payload is treated as const char* (a string literal
    // by convention; the cast is unsafe with non-static lifetimes -- see
    // the SFX comment block in AudioManager.h).
    mListenerSfxPlay = EventManager::Get().Subscribe("sfx_play",
        [this](const EventData& e) {
            if (e.payload)
                mSfx.PlaySfx(static_cast<const char*>(e.payload));
        });

    // Hit feedback is layered so attacks keep the readable old impact
    // sting while the newer short hit banks add moment-to-moment variety.
    // The old sting carries mix presence; the copied assets/Hits variants
    // carry texture and can be tuned or replaced in sfx.json.
    mListenerDamageTaken = EventManager::Get().Subscribe("battler_damage_taken",
        [this](const EventData& e) {
            const auto* payload = static_cast<const DamageTakenPayload*>(e.payload);
            mSfx.PlaySfx("battle_first_strike");
            mSfx.PlaySfx(payload && payload->isCrit
                ? "battle_hit_critical"
                : "battle_hit_physical");
        });

    mInitialized = true;
    LOG("[AudioManager] Initialized. %zu BGM track(s) loaded.", mTracks.size());
    return true;
}

// ============================================================
// Shutdown
// ============================================================

void AudioManager::Shutdown()
{
    if (!mInitialized &&
        !mMasterVoice &&
        !mBgmSubmix &&
        !mVoiceSubmix &&
        !mXAudio2.Get() &&
        !mCoInitialized)
    {
        return;
    }

    // Remove event subscriptions FIRST - prevents events fired during shutdown
    // (e.g., from state destructors) from calling into a partially torn-down manager.
    if (mListenerPlayOverworld >= 0)
    {
        EventManager::Get().Unsubscribe("bgm_play_overworld", mListenerPlayOverworld);
        mListenerPlayOverworld = -1;
    }
    if (mListenerPlayBattle >= 0)
    {
        EventManager::Get().Unsubscribe("bgm_play_battle", mListenerPlayBattle);
        mListenerPlayBattle = -1;
    }
    if (mListenerStop >= 0)
    {
        EventManager::Get().Unsubscribe("bgm_stop", mListenerStop);
        mListenerStop = -1;
    }
    if (mListenerBgmPlay >= 0)
    {
        EventManager::Get().Unsubscribe("bgm_play", mListenerBgmPlay);
        mListenerBgmPlay = -1;
    }
    if (mListenerSfxPlay >= 0)
    {
        EventManager::Get().Unsubscribe("sfx_play", mListenerSfxPlay);
        mListenerSfxPlay = -1;
    }
    if (mListenerDamageTaken >= 0)
    {
        EventManager::Get().Unsubscribe("battler_damage_taken", mListenerDamageTaken);
        mListenerDamageTaken = -1;
    }

    // Tear down the SFX subsystem before BGM voices so the engine graph
    // unwinds top-down: SFX source voices -> SFX submix -> BGM source
    // voices -> mastering voice -> engine.  Inverting this order risks
    // a data-race on the XAudio2 render thread.
    mSfx.Shutdown();

    // Stop and destroy all source voices.
    // Source voices MUST be destroyed before the mastering voice and the engine -
    // the engine graph flows from source -> master -> device; tearing it down in
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

    DestroyVoiceBus(mVoiceSubmix);
    DestroyVoiceBus(mBgmSubmix);

    // Mastering voice must be destroyed before IXAudio2 is released.
    if (mMasterVoice)
    {
        mMasterVoice->DestroyVoice();
        mMasterVoice = nullptr;
    }

    // ComPtr destructor calls IXAudio2::Release() - the engine is freed here.
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
// LoadBgmConfig - parse data/audio/bgm.json
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

    const size_t tracksKey = content.find("\"tracks\"");
    if (tracksKey == std::string::npos)
    {
        LOG("[AudioManager] WARNING: bgm.json has no tracks array.");
        return;
    }

    const size_t arrayStart = content.find('[', tracksKey);
    if (arrayStart == std::string::npos)
    {
        LOG("[AudioManager] WARNING: bgm.json tracks entry is not an array.");
        return;
    }

    const size_t arrayEnd = FindMatchingBracket(content, arrayStart);
    if (arrayEnd == std::string::npos)
    {
        LOG("[AudioManager] WARNING: bgm.json tracks array is malformed.");
        return;
    }

    // Parse each track object independently so optional fields, such as
    // loop, are scoped to the same entry instead of leaking across tracks.
    size_t pos = arrayStart + 1;
    while (pos < arrayEnd)
    {
        const size_t objectStart = content.find('{', pos);
        if (objectStart == std::string::npos || objectStart >= arrayEnd) break;

        const size_t objectEnd = FindMatchingBrace(content, objectStart);
        if (objectEnd == std::string::npos || objectEnd > arrayEnd)
        {
            LOG("[AudioManager] WARNING: skipping malformed BGM track object.");
            break;
        }

        const std::string objectText =
            content.substr(objectStart, objectEnd - objectStart + 1);

        std::string trackId;
        std::string trackPath;
        if (!ExtractStringValue(objectText, "id", trackId) ||
            !ExtractStringValue(objectText, "path", trackPath))
        {
            LOG("[AudioManager] WARNING: skipping BGM track with missing id or path.");
            pos = objectEnd + 1;
            continue;
        }

        const bool loop = ExtractBoolValueOrDefault(objectText, "loop", true);
        LoadTrack(trackId, trackPath, loop);
        pos = objectEnd + 1;
    }
}

// ============================================================
// LoadTrack
// ============================================================

bool AudioManager::LoadTrack(const std::string& id, const std::string& path, bool loop)
{
    TrackData track;
    track.loop = loop;

    // Choose loader by file extension.  .wav files use the lightweight
    // hand-rolled RIFF parser; everything else (mp3, wma, aac, flac, ...)
    // goes through Windows Media Foundation which auto-selects the OS
    // codec and decodes to 16-bit PCM.
    bool loaded = false;
    const std::string ext = (path.size() >= 4) ? path.substr(path.size() - 4) : "";
    if (ext == ".wav" || ext == ".WAV")
    {
        loaded = WavLoader::LoadFile(path, track.wfx, track.pcmData);
    }
    else
    {
        loaded = MediaLoader::LoadFile(path, track.wfx, track.pcmData);
    }

    if (!loaded)
    {
        LOG("[AudioManager] Failed to load track '%s' from '%s'.",
            id.c_str(), path.c_str());
        return false;
    }

    // Create the source voice with the decoded format descriptor.
    // BGM voices route through the BGM submix so one settings value can
    // control every music track without touching SFX or future voice audio.
    XAUDIO2_SEND_DESCRIPTOR sendDesc = {};
    XAUDIO2_VOICE_SENDS sends = {};
    XAUDIO2_VOICE_SENDS* sendsPtr = nullptr;
    if (mBgmSubmix)
    {
        sendDesc.pOutputVoice = mBgmSubmix;
        sends.SendCount = 1;
        sends.pSends = &sendDesc;
        sendsPtr = &sends;
    }

    // The voice is created once and reused across all play/stop cycles -
    // voice creation is expensive; reuse avoids per-play allocation overhead.
    const HRESULT hr = mXAudio2->CreateSourceVoice(
        &track.voice,
        &track.wfx,
        0,                          // flags
        XAUDIO2_DEFAULT_FREQ_RATIO, // max pitch ratio (2.0 = one octave up)
        nullptr,                    // callback (none needed for BGM)
        sendsPtr,                   // send list (BGM bus when available)
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
    LOG("[AudioManager] Loaded BGM track '%s' (%zu bytes PCM, loop=%s).",
        id.c_str(), mTracks[id].pcmData.size(), mTracks[id].loop ? "true" : "false");
    return true;
}

bool AudioManager::CreateSubmixBus(IXAudio2SubmixVoice** outVoice, const char* label)
{
    if (!outVoice || !mXAudio2.Get() || !mMasterVoice) return false;

    XAUDIO2_VOICE_DETAILS masterDetails = {};
    mMasterVoice->GetVoiceDetails(&masterDetails);

    const HRESULT hr = mXAudio2->CreateSubmixVoice(
        outVoice,
        masterDetails.InputChannels,
        masterDetails.InputSampleRate,
        0,
        0,
        nullptr,
        nullptr);

    if (FAILED(hr))
    {
        LOG("[AudioManager] CreateSubmixVoice failed for %s bus (0x%08X).",
            label ? label : "unknown", hr);
        *outVoice = nullptr;
        return false;
    }

    LOG("[AudioManager] Created %s audio bus.", label ? label : "unknown");
    return true;
}

void AudioManager::DestroyVoiceBus(IXAudio2SubmixVoice*& voice)
{
    if (!voice) return;

    voice->DestroyVoice();
    voice = nullptr;
}

// ============================================================
// PlayBGM
// ============================================================

void AudioManager::PlayBGM(const std::string& trackId)
{
    // Idempotent for active playback: looping tracks never restart, and
    // one-shot tracks only replay after XAudio2 reports that no submitted
    // buffer remains queued.
    if (mCurrentTrackId == trackId)
    {
        auto currentIt = mTracks.find(mCurrentTrackId);
        if (currentIt != mTracks.end() && currentIt->second.voice)
        {
            if (currentIt->second.loop) return;

            XAUDIO2_VOICE_STATE state = {};
            currentIt->second.voice->GetState(&state);
            if (state.BuffersQueued > 0) return;
        }

        StopBGM();
    }

    // Stop the currently playing track (if any) before switching.
    StopBGM();

    auto it = mTracks.find(trackId);
    if (it == mTracks.end() || !it->second.loaded)
    {
        LOG("[AudioManager] PlayBGM: unknown or unloaded track '%s'.", trackId.c_str());
        return;
    }

    TrackData& track = it->second;

    // Build an XAUDIO2_BUFFER using the track policy from bgm.json.
    // LoopBegin=0, LoopLength=0 means "loop the whole buffer" when looped.
    // Flags=XAUDIO2_END_OF_STREAM signals the end of the stream to XAudio2
    // so it can make scheduling decisions. LoopCount=0 plays once.
    XAUDIO2_BUFFER buffer = {};
    buffer.Flags      = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(track.pcmData.size());
    buffer.pAudioData = track.pcmData.data();
    buffer.LoopCount  = track.loop ? XAUDIO2_LOOP_INFINITE : 0;

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
    LOG("[AudioManager] Playing BGM: '%s' (loop=%s).",
        trackId.c_str(), track.loop ? "true" : "false");
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
        // the submitted buffer so SubmitSourceBuffer() starts from byte 0 next time.
        it->second.voice->Stop(0);
        it->second.voice->FlushSourceBuffers();
    }

    mCurrentTrackId.clear();
}

// ============================================================
// SFX facade -- delegates to mSfx, but guards against use before
// Initialize() so callers do not need to test IsInitialized() first.
// ============================================================

void AudioManager::PlaySfx(const std::string& groupId, float volumeMul)
{
    if (!mInitialized) return;
    mSfx.PlaySfx(groupId, volumeMul);
}

void AudioManager::SetBgmMasterVolume(float v)
{
    mBgmMasterVolume = ClampVolume(v);
    if (mBgmSubmix)
    {
        const HRESULT hr = mBgmSubmix->SetVolume(mBgmMasterVolume);
        if (FAILED(hr))
        {
            LOG("[AudioManager] Failed to set BGM volume (0x%08X).", hr);
        }
    }
}

void AudioManager::SetSfxMasterVolume(float v)
{
    mSfx.SetMasterVolume(v);
}

float AudioManager::GetSfxMasterVolume() const
{
    return mSfx.GetMasterVolume();
}

void AudioManager::SetVoiceMasterVolume(float v)
{
    mVoiceMasterVolume = ClampVolume(v);
    if (mVoiceSubmix)
    {
        const HRESULT hr = mVoiceSubmix->SetVolume(mVoiceMasterVolume);
        if (FAILED(hr))
        {
            LOG("[AudioManager] Failed to set Voice volume (0x%08X).", hr);
        }
    }
}
