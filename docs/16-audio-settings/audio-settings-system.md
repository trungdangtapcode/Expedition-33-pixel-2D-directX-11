# Audio Settings System

## Summary

The title menu Options screen exposes global audio settings for:

- BGM
- SFX
- Voice

The settings are global preferences, not save-slot state. They live in:

```json
save/settings.json
```

Current schema:

```json
{
  "language": "en_us",
  "bgmVolume": 1.0,
  "sfxVolume": 1.0,
  "voiceVolume": 1.0
}
```

Missing values migrate safely to `1.0`. All volume values are clamped to
`0.0..1.0` before they are stored or applied.

## Audio Buses

`AudioManager` owns the XAudio2 engine, the mastering voice, and two extra
submix buses:

- BGM bus: every looping music source voice routes here.
- Voice bus: reserved for future cutscene and dialogue playback.

`SfxPlayer` owns the SFX submix bus. It still reads the authored
`masterSfxVolume` from `data/audio/sfx.json`, then multiplies that baseline by
the user SFX volume from `save/settings.json`.

This keeps three concerns separate:

- Per-sound author mix values in audio data.
- Per-player preferences in settings.
- Runtime XAudio2 routing in audio code.

## Menu Behavior

The main menu Options order is:

1. Language
2. BGM Volume
3. SFX Volume
4. Voice Volume
5. Back

Left and Right adjust a selected volume row in 10 percent steps. Enter on a
volume row increases by 10 percent and wraps from 100 percent back to 0 percent.
Every change saves immediately and updates the live audio graph.

Voice is visible now even though voice playback is future content. The row is
marked with a subtle Future tag, remains selectable, and persists like the other
audio settings so no future settings migration is needed.

## Visual Layout

The Options screen uses a dedicated lower title-panel layout instead of the
plain centered command list. Labels, values, meter dimensions, row spacing, and
panel opacity are configured in:

```text
data/main_menu_layout.json
```

Volume rows use ten compact meter segments. Filled segments represent the
current percentage in 10 percent increments; empty segments use low-alpha gray.

## Public Interfaces

`SettingsManager`:

- `GetBgmVolume()`
- `GetSfxVolume()`
- `GetVoiceVolume()`
- `SetBgmVolume(float)`
- `SetSfxVolume(float)`
- `SetVoiceVolume(float)`

`AudioManager`:

- `SetBgmMasterVolume(float)`
- `GetBgmMasterVolume()`
- `SetSfxMasterVolume(float)`
- `GetSfxMasterVolume()`
- `SetVoiceMasterVolume(float)`
- `GetVoiceMasterVolume()`

## Verification

Build:

```bat
.\build_src_static.bat 2>&1
```

Expected successful tail:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

Manual checks:

- Delete `save/settings.json`; defaults are recreated.
- Load an old settings file without `voiceVolume`; Voice defaults to 100 percent.
- Change BGM volume on the title menu; menu music changes immediately.
- Change SFX volume; menu feedback changes immediately.
- Set SFX to 0 percent; UI input still works while SFX is silent.
- Change Voice volume; value persists and the row remains marked as Future.
