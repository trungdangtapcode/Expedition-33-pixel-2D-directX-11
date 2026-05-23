# SFX System

## Overview

The SFX (Sound Effects) system provides one-shot audio feedback for UI
interactions, battle events, and gameplay triggers. It is built on top of
XAudio2 with a voice-pool architecture that supports overlapping playback
and randomised variant selection to prevent ear fatigue.

The system is split across two layers:

| Layer | Files | Responsibility |
|---|---|---|
| Infrastructure | `SfxPlayer.h/.cpp`, `WavLoader.h/.cpp`, `MediaLoader.h/.cpp` | Voice pools, PCM loading, format decoding |
| Integration | `AudioManager.h/.cpp` | Singleton facade, event subscriptions, public `PlaySfx()` API |

All SFX configuration is data-driven via `data/audio/sfx.json` -- adding or
changing sounds requires no recompilation.

---

## Raw Asset Audit

The raw sound library is much larger than the runtime catalog:

| Folder | WAV count | Notes |
|---|---:|---|
| `assets/ORIGINAL_sound` | 262 | Battle UI, QTE, rest menu, dialogue, cinematic cues |
| `assets/Hits` | 135 | Weapon, magic, blunt, feedback, and custom character hit impacts |
| `assets/sound/SFX` before this pass | 44 | Curated runtime subset used by the game |

The game intentionally does not load directly from `assets/ORIGINAL_sound`
or `assets/Hits`. Those folders are treated as raw libraries. Any sound used
by the game is copied into `assets/sound/SFX/...` and referenced from
`data/audio/sfx.json`. This keeps runtime asset paths stable and makes it
clear which source assets are production-ready.

---

## Architecture

### Voice Pools

XAudio2 source voices have a fixed format set at creation time. A voice can
only play one buffer at a time; submitting a second queues it (no overlap).
To support overlapping SFX (e.g. rapid menu clicks), the system preallocates
pools of voices keyed by channel count:

```
VoicePool
  format: WAVEFORMATEX       (shared by all voices in the pool)
  voices: IXAudio2SourceVoice*[]   (preallocated, e.g. 16 stereo + 4 mono)
  nextIdx: size_t             (round-robin cursor)
```

**Allocation rule per `PlaySfx` call:**
1. Match the SFX format to the correct pool (stereo or mono).
2. Walk the pool starting at `nextIdx`; return the first voice with
   `BuffersQueued == 0` (idle).
3. If no voice is idle, **steal the oldest** -- `Stop(0)` +
   `FlushSourceBuffers`. This is the standard voice-stealing pattern;
   rare in practice with 16 stereo voices.
4. `SubmitSourceBuffer` + `Start`. Advance `nextIdx`.

Pool sizes are configured in `sfx.json` (`stereoVoiceCount`, `monoVoiceCount`).

### SFX Groups (Randomised Variants)

Many UI sounds ship as 5--10 variants (e.g. `navigate_01` through
`navigate_10`). The system picks one randomly per call, avoiding back-to-back
repeats:

```
SfxGroup
  pcmBuffers: vector<vector<BYTE>>   (one per variant)
  format: WAVEFORMATEX                (all variants share format)
  volume: float                       (per-group gain, 0..1)
  lastPickedIndex: size_t             (avoid two-in-a-row)
```

### Submix Bus

All SFX source voices route through a single **SFX submix voice** before
reaching the mastering voice. This lets `SetSfxMasterVolume(float)` control
all SFX with one call, independent of BGM and future voice volume.

`masterSfxVolume` in `sfx.json` is the authored baseline for the SFX bus.
The user setting from `save/settings.json` multiplies that baseline at runtime.

```
Source voices --> SFX Submix Voice --> Mastering Voice --> Speakers
                                  ^
                   authored baseline * user SFX volume
```

### Audio Format Support

| Extension | Loader | Notes |
|---|---|---|
| `.wav` | `WavLoader` | Hand-rolled RIFF parser. PCM and IEEE float. |
| `.mp3`, `.wma`, `.aac`, `.flac`, `.ogg` | `MediaLoader` | Windows Media Foundation. Auto-selects OS codec. |

`AudioManager::LoadTrack` auto-detects format by file extension. WAV files
use the lightweight parser; everything else goes through Media Foundation.
Both loaders output the same `{WAVEFORMATEX, vector<BYTE>}` pair.

---

## Configuration: `data/audio/sfx.json`

```json
{
  "stereoVoiceCount": 16,
  "monoVoiceCount": 4,
  "masterSfxVolume": 1.0,
  "groups": [
    {
      "id": "ui_navigate",
      "volume": 0.6,
      "paths": [
        "assets/sound/SFX/UI/navigate_01.wav",
        "assets/sound/SFX/UI/navigate_02.wav"
      ]
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `stereoVoiceCount` | int | Preallocated stereo voice pool size |
| `monoVoiceCount` | int | Preallocated mono voice pool size |
| `masterSfxVolume` | float | Authored SFX bus baseline (0..1), multiplied by user SFX volume |
| `groups[].id` | string | Group key used by `PlaySfx("id")` |
| `groups[].volume` | float | Per-group volume (0..1) |
| `groups[].paths` | string[] | WAV file paths (all must share format) |

### Adding a New SFX

1. Place WAV files in `assets/sound/SFX/<category>/`.
2. Add a group entry to `sfx.json` with an `id` and `paths`.
3. Call `AudioManager::Get().PlaySfx("your_id")` from code.
4. No recompilation needed if the group id already has a trigger site.

---

## Public API

### Direct Call (Recommended)

```cpp
#include "../Audio/AudioManager.h"

AudioManager::Get().PlaySfx("ui_navigate");
AudioManager::Get().PlaySfx("battle_start", 0.5f);  // 50% volume override
```

### Event Bus (For Decoupled Systems)

```cpp
EventData e;
e.payload = const_cast<char*>("ui_navigate");
EventManager::Get().Broadcast("sfx_play", e);
```

The event path avoids an `AudioManager.h` dependency for systems that
already use `EventManager`.

---

## Trigger Sites

### UI Navigation

| Trigger | SFX Group | File |
|---|---|---|
| Cursor up/down in any menu | `ui_navigate` | `BattleInputController.cpp`, `InventoryState.cpp`, `LineupState.cpp` |
| Confirm selection | `ui_confirm` | Same files + `MenuState.cpp` |
| Back / cancel | `ui_back` | Same files |
| Tab switch (inventory) | `ui_navigate` | `InventoryState.cpp` |
| Q/E party member cycle | `ui_navigate` | `InventoryState.cpp` |
| Invalid action (HP full, skill unavailable) | `battle_no_ap` | `BattleInputController.cpp`, `InventoryState.cpp` |

### Battle Commands

| Trigger | SFX Group | File |
|---|---|---|
| Fight (skill menu open) | `battle_skill_open` | `FightCommand.cpp` |
| Item (item menu open) | `battle_item_open` | `ItemCommand.cpp` |
| Flee | `battle_flee` | `FleeCommand.cpp` |
| Target cycle (enemy) | `battle_select_enemy` | `BattleInputController.cpp` |
| Target cycle (ally, item) | `battle_select_ally` | `BattleInputController.cpp` |

### Battle Events

| Trigger | SFX Group | File |
|---|---|---|
| Battle encounter start | `battle_start` | `BattleState.cpp` |
| Damage dealt (mix-presence layer) | `battle_first_strike` | `AudioManager.cpp` (event subscriber) |
| Damage dealt (normal texture layer) | `battle_hit_physical` | `AudioManager.cpp` (event subscriber) |
| Damage dealt (critical texture layer) | `battle_hit_critical` | `AudioManager.cpp` (event subscriber) |

### QTE Events

QTE timing and SFX group selection are routed through
`data/battle_system_config.json`:

```json
{
  "qteStartSfxId": "battle_qte_start",
  "qteMissSfxId": "battle_qte_miss",
  "qteGoodSfxId": "battle_qte_good",
  "qtePerfectSfxId": "battle_qte_perfect"
}
```

`QteAnimDamageAction` reads those fields through the live `BattleContext`.
It broadcasts `sfx_play` when the QTE window starts and when each node
resolves. The action never stores file paths, so replacing the QTE sound set
is a data edit only.

| Trigger | SFX Group | Source assets copied into runtime tree |
|---|---|---|
| QTE window starts | `battle_qte_start` | `assets/ORIGINAL_sound/Battle/UI_Battle_QTE/UI_Battle_QTE_Start.wav` |
| QTE miss | `battle_qte_miss` | `assets/ORIGINAL_sound/Battle/UI_Battle_QTE/UI_Battle_QTE_Miss.wav` |
| QTE good | `battle_qte_good` | regular success variants from `assets/ORIGINAL_sound/Battle/...` |
| QTE perfect | `battle_qte_perfect` | perfect validation variants from `assets/ORIGINAL_sound/Battle/...` |

### Hit Impact Expansion

The runtime catalog now exposes more impact banks copied from `assets/Hits`:

| SFX Group | Runtime files | Intended use |
|---|---|---|
| `battle_first_strike` | `assets/sound/SFX/Battle/first_strike_01..03.wav` | Main audible hit layer |
| `battle_hit_physical` | `assets/sound/SFX/Hits/Sword/sword_medium_01..08.wav` | Standard weapon hits |
| `battle_hit_magic` | `assets/sound/SFX/Hits/Magic/magic_generic_01..04.wav` | Future magical attacks |
| `battle_hit_blunt` | `assets/sound/SFX/Hits/Blunt/hammer_medium_01..06.wav` | Future hammer or impact attacks |
| `battle_hit_critical` | `assets/sound/SFX/Hits/Feedback/critical.wav` | Critical hit accent |
| `battle_hit_weakness` | `assets/sound/SFX/Hits/Feedback/weakness_01..02.wav` | Future weakness feedback |

---

## Per-Encounter BGM

Encounters can override the default battle BGM via a `bgmTrackId` field
in the enemy JSON:

```json
{
  "name": "Boss",
  "bgmTrackId": "boss_theme",
  ...
}
```

The track must be registered in `data/audio/bgm.json`:

```json
{ "id": "boss_theme", "path": "assets/sound/OST/boss.mp3" }
```

If `bgmTrackId` is empty or absent, `BattleState` plays the default
`"battle"` track.

Current encounter-specific tracks:

| Encounter | `bgmTrackId` | Track path |
|---|---|---|
| Zombie Armour | `zombie_armour_battle` | `assets/sound/OST/beneath_the_blue_tree.mp3` |
| Verso Cloned | `verso_cloned_battle` | `assets/sound/OST/verso-cloned-fight.mp3` |

---

## Asset Directory Layout

```
assets/sound/
  OST/                          BGM tracks (.wav or .mp3)
  SFX/
    UI/                         UI interaction sounds
      navigate_01..10.wav       Cursor movement (10 randomised variants)
      back_01..10.wav           Cancel / back
      confirm_01..10.wav        Confirm / select
    Battle/                     Battle-specific one-shots
      start.wav                 Encounter sting
      first_strike_01..03.wav   Hit impact (3 variants)
      QTE/
        start.wav               QTE prompt appears
        miss.wav                QTE missed
        success_good_01..02.wav Regular QTE success
        success_perfect_01..02.wav Perfect QTE success
      select_ally_01..03.wav    Ally target cursor
      select_enemy_01..03.wav   Enemy target cursor
      skill_open.wav            Skill sub-menu open
      item_open.wav             Item sub-menu open
      no_ap.wav                 Action denied buzzer
      flee.wav                  Flee activation
    Hits/
      Sword/sword_medium_01..08.wav
      Magic/magic_generic_01..04.wav
      Blunt/hammer_medium_01..06.wav
      Feedback/critical.wav
      Feedback/weakness_01..02.wav
```

---

## Lifetime and Shutdown

```
Initialize:
  CoInit -> XAudio2Create -> MasterVoice -> SfxSubmixVoice
  -> preload SFX groups -> preallocate voice pools -> subscribe events

Shutdown:
  Unsubscribe events -> DestroyVoice on every pool voice -> pool.clear
  -> DestroyVoice SfxSubmixVoice -> DestroyVoice MasterVoice
  -> mXAudio2.Reset -> CoUninit
```

PCM buffers in `mSfxGroups` must outlive all source voices -- they are
kept alive for the engine's entire lifetime.
