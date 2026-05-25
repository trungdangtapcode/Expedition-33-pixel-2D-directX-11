// ============================================================
// File: SfxPlayer.cpp
// Responsibility: Implement the one-shot SFX subsystem.  Loads
//                 sfx.json, builds voice pools, and plays groups
//                 by id with random variant selection.
// ============================================================
#include "SfxPlayer.h"
#include "WavLoader.h"
#include "../Utils/Log.h"

#include <fstream>
#include <random>
#include <cstring>

// ============================================================
// Tiny helpers (file-local)
// ============================================================

namespace
{
    // Same hand-written JSON-key scanner pattern AudioManager::LoadBgmConfig
    // uses.  Returns empty string when key is missing -- callers treat empty
    // as "field not present, use default".
    std::string ExtractStringField(const std::string& src, size_t fromPos,
                                   const std::string& key)
    {
        const std::string needle = "\"" + key + "\"";
        const size_t k = src.find(needle, fromPos);
        if (k == std::string::npos) return {};

        size_t q1 = src.find('"', k + needle.size());
        if (q1 == std::string::npos) return {};
        ++q1;
        const size_t q2 = src.find('"', q1);
        if (q2 == std::string::npos) return {};
        return src.substr(q1, q2 - q1);
    }

    // Pull a numeric field by scanning for "<key>": <number>.  Returns
    // 'fallback' when the key is missing or the number cannot be parsed.
    float ExtractNumberField(const std::string& src, size_t fromPos,
                             const std::string& key, float fallback)
    {
        const std::string needle = "\"" + key + "\"";
        const size_t k = src.find(needle, fromPos);
        if (k == std::string::npos) return fallback;

        // Skip past the colon to the value.  Whitespace tolerated.
        const size_t colon = src.find(':', k + needle.size());
        if (colon == std::string::npos) return fallback;

        size_t p = colon + 1;
        while (p < src.size() && (src[p] == ' ' || src[p] == '\t')) ++p;

        // The number ends at the first comma, brace, or whitespace.
        size_t e = p;
        while (e < src.size()
               && src[e] != ','
               && src[e] != '}'
               && src[e] != ']'
               && src[e] != '\n'
               && src[e] != '\r'
               && src[e] != ' ')
        {
            ++e;
        }
        if (e == p) return fallback;

        try { return std::stof(src.substr(p, e - p)); }
        catch (...) { return fallback; }
    }

    // Random engine shared across PlaySfx() calls.  Stays warm so we do
    // not seed from the clock on every keypress.  Game-thread only --
    // SfxPlayer documents that PlaySfx is not callable from audio threads.
    std::mt19937& Rng()
    {
        static std::mt19937 engine{ std::random_device{}() };
        return engine;
    }
}

// ============================================================
// Initialize
// ============================================================

bool SfxPlayer::Initialize(IXAudio2*               engine,
                           IXAudio2MasteringVoice* master,
                           const std::string&      configPath)
{
    if (!engine || !master)
    {
        LOG("[SfxPlayer] Initialize: engine or master is null. SFX disabled.");
        return false;
    }
    mEngine = engine;

    // Submix voice routes every SFX source through one node so a single
    // SetVolume on mSubmix scales the whole SFX bus.  Channel count and
    // sample rate match the master voice's input format -- 0/0 here means
    // "use the master's defaults", which is what we want.
    XAUDIO2_VOICE_DETAILS masterDetails = {};
    master->GetVoiceDetails(&masterDetails);

    HRESULT hr = mEngine->CreateSubmixVoice(
        &mSubmix,
        masterDetails.InputChannels,
        masterDetails.InputSampleRate,
        0,           // flags
        0,           // processing stage (0 = before master)
        nullptr,     // send list (defaults to master)
        nullptr);    // effect chain
    if (FAILED(hr))
    {
        LOG("[SfxPlayer] CreateSubmixVoice failed (0x%08lX). SFX disabled.",
            static_cast<unsigned long>(hr));
        return false;
    }

    // Read the config file BEFORE creating pools, because the config
    // optionally overrides the default pool sizes.
    if (!LoadConfig(configPath))
    {
        LOG("[SfxPlayer] LoadConfig('%s') failed -- no SFX will play.",
            configPath.c_str());
        // Still return true: no SFX is a degraded but valid state.
    }

    // Build the two pools.  Both formats are 16-bit PCM 48kHz; the only
    // difference is channel count.  These constants match what every
    // asset in assets/ORIGINAL_sound declares.
    WAVEFORMATEX stereoFmt = {};
    stereoFmt.wFormatTag      = WAVE_FORMAT_PCM;
    stereoFmt.nChannels       = 2;
    stereoFmt.nSamplesPerSec  = 48000;
    stereoFmt.wBitsPerSample  = 16;
    stereoFmt.nBlockAlign     = stereoFmt.nChannels * stereoFmt.wBitsPerSample / 8;
    stereoFmt.nAvgBytesPerSec = stereoFmt.nSamplesPerSec * stereoFmt.nBlockAlign;

    WAVEFORMATEX monoFmt = stereoFmt;
    monoFmt.nChannels       = 1;
    monoFmt.nBlockAlign     = monoFmt.nChannels * monoFmt.wBitsPerSample / 8;
    monoFmt.nAvgBytesPerSec = monoFmt.nSamplesPerSec * monoFmt.nBlockAlign;

    if (!CreatePool(mStereoPool, stereoFmt, mStereoVoiceCount))
    {
        LOG("[SfxPlayer] Stereo voice pool creation failed.");
    }
    if (!CreatePool(mMonoPool, monoFmt, mMonoVoiceCount))
    {
        LOG("[SfxPlayer] Mono voice pool creation failed.");
    }

    mInitialized = true;
    LOG("[SfxPlayer] Initialized. %zu group(s) loaded; %d stereo + %d mono voices.",
        mGroups.size(),
        static_cast<int>(mStereoPool.voices.size()),
        static_cast<int>(mMonoPool.voices.size()));
    return true;
}

// ============================================================
// Shutdown -- order is critical: source voices first, then submix.
// ============================================================

void SfxPlayer::Shutdown()
{
    if (!mInitialized) return;

    auto destroyPool = [](VoicePool& pool)
    {
        for (auto* v : pool.voices)
        {
            if (!v) continue;
            v->Stop(0);
            v->FlushSourceBuffers();
            v->DestroyVoice();
        }
        pool.voices.clear();
        pool.nextIdx = 0;
    };
    destroyPool(mStereoPool);
    destroyPool(mMonoPool);

    if (mSubmix)
    {
        mSubmix->DestroyVoice();
        mSubmix = nullptr;
    }

    // PCM byte buffers in mGroups are released here; their voices are gone
    // so no audio thread can be reading them anymore.
    mGroups.clear();

    mEngine = nullptr;
    mInitialized = false;
}

// ============================================================
// LoadConfig -- parse data/audio/sfx.json
// ============================================================

bool SfxPlayer::LoadConfig(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        LOG("[SfxPlayer] WARNING: sfx.json not found at '%s'.", path.c_str());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // Top-level numeric overrides.  Defaults match the field initialisers
    // on the class, so a missing field keeps the default.
    mStereoVoiceCount = static_cast<int>(
        ExtractNumberField(content, 0, "stereoVoiceCount",
                           static_cast<float>(mStereoVoiceCount)));
    mMonoVoiceCount = static_cast<int>(
        ExtractNumberField(content, 0, "monoVoiceCount",
                           static_cast<float>(mMonoVoiceCount)));
    mConfigMasterVolume = ExtractNumberField(content, 0, "masterSfxVolume", 1.0f);
    ApplyMasterVolume();

    // Walk every group object.  Each one starts with an "id" key followed
    // by a "paths" array.  We scan the array brackets manually so we do
    // not have to write a real JSON parser for one config file.
    size_t cursor = content.find("\"groups\"");
    if (cursor == std::string::npos)
    {
        LOG("[SfxPlayer] sfx.json has no 'groups' array.");
        return false;
    }

    while (true)
    {
        const size_t idKey = content.find("\"id\"", cursor);
        if (idKey == std::string::npos) break;

        const std::string id = ExtractStringField(content, idKey, "id");
        if (id.empty()) break;

        // Per-group volume defaults to 1.0.  Bound the search to the
        // current group object to avoid leaking the next group's volume.
        const size_t pathsKey = content.find("\"paths\"", idKey);
        if (pathsKey == std::string::npos) break;
        const float groupVolume =
            ExtractNumberField(content, idKey, "volume", 1.0f);

        // Find the [...] array bounds for "paths".
        const size_t lbracket = content.find('[', pathsKey);
        const size_t rbracket = (lbracket != std::string::npos)
            ? content.find(']', lbracket) : std::string::npos;
        if (lbracket == std::string::npos || rbracket == std::string::npos)
        {
            LOG("[SfxPlayer] Group '%s' has malformed 'paths' array.", id.c_str());
            cursor = idKey + 4;
            continue;
        }

        // Pull every quoted string between the brackets.
        std::vector<std::string> paths;
        size_t scan = lbracket + 1;
        while (scan < rbracket)
        {
            const size_t q1 = content.find('"', scan);
            if (q1 == std::string::npos || q1 >= rbracket) break;
            const size_t q2 = content.find('"', q1 + 1);
            if (q2 == std::string::npos || q2 >= rbracket) break;
            paths.emplace_back(content.substr(q1 + 1, q2 - q1 - 1));
            scan = q2 + 1;
        }

        LoadGroup(id, paths, groupVolume);
        cursor = rbracket + 1;
    }

    return !mGroups.empty();
}

// ============================================================
// LoadGroup -- decode every WAV variant into the group's PCM list
// ============================================================

bool SfxPlayer::LoadGroup(const std::string&              id,
                          const std::vector<std::string>& paths,
                          float                           volume)
{
    if (paths.empty())
    {
        LOG("[SfxPlayer] Group '%s' has no paths -- skipping.", id.c_str());
        return false;
    }

    SfxGroup group;
    group.volume = volume;

    bool fmtSet = false;
    for (const std::string& p : paths)
    {
        WAVEFORMATEX wfx = {};
        std::vector<BYTE> pcm;
        if (!WavLoader::LoadFile(p, wfx, pcm))
        {
            LOG("[SfxPlayer] Group '%s': failed to load '%s' -- skipping variant.",
                id.c_str(), p.c_str());
            continue;
        }

        if (!fmtSet)
        {
            group.format = wfx;
            fmtSet = true;
        }
        else if (wfx.nChannels       != group.format.nChannels
              || wfx.nSamplesPerSec  != group.format.nSamplesPerSec
              || wfx.wBitsPerSample  != group.format.wBitsPerSample
              || wfx.wFormatTag      != group.format.wFormatTag)
        {
            // Mismatched formats inside one group would force per-variant
            // voice format selection -- not worth the complexity.  Reject
            // the variant so the group stays consistent.
            LOG("[SfxPlayer] Group '%s': '%s' has mismatched format -- skipping.",
                id.c_str(), p.c_str());
            continue;
        }

        group.pcmBuffers.push_back(std::move(pcm));
    }

    if (group.pcmBuffers.empty())
    {
        LOG("[SfxPlayer] Group '%s' had no usable variants.", id.c_str());
        return false;
    }

    mGroups.emplace(id, std::move(group));
    return true;
}

// ============================================================
// CreatePool -- preallocate N voices, all pointing at the submix
// ============================================================

bool SfxPlayer::CreatePool(VoicePool& pool, const WAVEFORMATEX& fmt, int count)
{
    pool.format  = fmt;
    pool.nextIdx = 0;
    pool.voices.reserve(count);

    // Send list: every SFX voice routes to mSubmix instead of master.
    XAUDIO2_SEND_DESCRIPTOR sendDesc = {};
    sendDesc.pOutputVoice = mSubmix;
    XAUDIO2_VOICE_SENDS sends = { 1, &sendDesc };

    for (int i = 0; i < count; ++i)
    {
        IXAudio2SourceVoice* v = nullptr;
        const HRESULT hr = mEngine->CreateSourceVoice(
            &v,
            &fmt,
            0,                          // flags
            XAUDIO2_DEFAULT_FREQ_RATIO,
            nullptr,                    // callback
            &sends,
            nullptr);                   // effect chain
        if (FAILED(hr) || !v)
        {
            LOG("[SfxPlayer] CreateSourceVoice failed in pool (ch=%u, hr=0x%08lX).",
                fmt.nChannels, static_cast<unsigned long>(hr));
            continue;
        }
        pool.voices.push_back(v);
    }
    return !pool.voices.empty();
}

// ============================================================
// PickPool / AcquireVoice -- pool selection + round-robin/steal
// ============================================================

SfxPlayer::VoicePool* SfxPlayer::PickPool(const WAVEFORMATEX& fmt)
{
    if (fmt.nChannels == 1) return &mMonoPool;
    if (fmt.nChannels == 2) return &mStereoPool;
    return nullptr;
}

IXAudio2SourceVoice* SfxPlayer::AcquireVoice(VoicePool& pool)
{
    if (pool.voices.empty()) return nullptr;

    const size_t n = pool.voices.size();

    // Walk forward from nextIdx looking for an idle voice (no buffers
    // queued or playing).  XAUDIO2_VOICE_STATE.BuffersQueued is the
    // standard idle marker.
    for (size_t step = 0; step < n; ++step)
    {
        const size_t i = (pool.nextIdx + step) % n;
        XAUDIO2_VOICE_STATE state = {};
        pool.voices[i]->GetState(&state);
        if (state.BuffersQueued == 0)
        {
            pool.nextIdx = (i + 1) % n;
            return pool.voices[i];
        }
    }

    // Every voice busy.  Steal the one at nextIdx -- it is the oldest
    // by round-robin order.  Voice stealing is the standard UI pattern;
    // the alternative ("drop new sound") feels broken on rapid clicks.
    IXAudio2SourceVoice* v = pool.voices[pool.nextIdx];
    v->Stop(0);
    v->FlushSourceBuffers();
    pool.nextIdx = (pool.nextIdx + 1) % n;
    return v;
}

// ============================================================
// PlaySfx -- the one public method state code calls
// ============================================================

void SfxPlayer::PlaySfx(const std::string& groupId, float volumeMul)
{
    if (!mInitialized) return;

    auto it = mGroups.find(groupId);
    if (it == mGroups.end())
    {
        LOG("[SfxPlayer] PlaySfx: unknown group '%s'.", groupId.c_str());
        return;
    }
    SfxGroup& group = it->second;
    if (group.pcmBuffers.empty()) return;

    // Pick a variant.  When a group has 2+ variants, avoid replaying
    // the most recent one so the same click never fires back-to-back.
    size_t pick;
    if (group.pcmBuffers.size() == 1)
    {
        pick = 0;
    }
    else
    {
        std::uniform_int_distribution<size_t> dist(0, group.pcmBuffers.size() - 1);
        do { pick = dist(Rng()); } while (pick == group.lastPickedIndex);
    }
    group.lastPickedIndex = pick;

    VoicePool* pool = PickPool(group.format);
    if (!pool)
    {
        LOG("[SfxPlayer] PlaySfx '%s': no pool for %u-channel audio.",
            groupId.c_str(), group.format.nChannels);
        return;
    }

    IXAudio2SourceVoice* voice = AcquireVoice(*pool);
    if (!voice) return;

    // Effective per-voice gain = group volume times per-call multiplier.
    // Master SFX gain lives on mSubmix and applies on top in the mixer.
    voice->SetVolume(group.volume * volumeMul);

    XAUDIO2_BUFFER buf = {};
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.AudioBytes = static_cast<UINT32>(group.pcmBuffers[pick].size());
    buf.pAudioData = group.pcmBuffers[pick].data();
    buf.LoopCount  = 0;   // one-shot, never loops

    HRESULT hr = voice->SubmitSourceBuffer(&buf);
    if (FAILED(hr))
    {
        LOG("[SfxPlayer] SubmitSourceBuffer failed for '%s' (0x%08lX).",
            groupId.c_str(), static_cast<unsigned long>(hr));
        return;
    }

    // Start(0) begins playback at the next render quantum -- low latency
    // (~10ms) which is well under the perceptual UI-feedback threshold.
    voice->Start(0);
}

// ============================================================
// Master volume
// ============================================================

void SfxPlayer::SetMasterVolume(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    mUserMasterVolume = v;
    ApplyMasterVolume();
}

void SfxPlayer::ApplyMasterVolume()
{
    float config = mConfigMasterVolume;
    if (config < 0.0f) config = 0.0f;
    if (config > 1.0f) config = 1.0f;

    if (mSubmix) mSubmix->SetVolume(config * mUserMasterVolume);
}
