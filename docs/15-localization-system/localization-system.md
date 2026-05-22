# Localization System

## Summary

The game now has a JSON-based localization path for player-facing text. The
active language is a global setting, not save-slot gameplay state, so the title
menu can render in the selected language before any save slot is loaded.

## Runtime Ownership

`SettingsManager` owns global player preferences and persists them in:

```json
save/settings.json
```

The first version stores:

```json
{
  "language": "en_us",
  "bgmVolume": 1.0,
  "sfxVolume": 1.0
}
```

`GameApp::Initialize()` initializes `SettingsManager` first, then initializes
`LocalizationManager` with the saved language id before `MenuState` is pushed.

## Localization Files

Language metadata lives in:

```text
data/localization/languages.json
```

Each language has a flat UTF-8 string table:

```text
data/localization/en_us.json
data/localization/fr_fr.json
data/localization/vi_vn.json
```

Keys use dotted names such as:

```text
menu.new_game
campfire.save_slot
battle.log.attack
item.potion_small.name
```

Keep C++ source, comments, logs, and documentation in English. Non-English
display strings belong only in localization data files.

## Fallback Rules

`LocalizationManager::Text(key)` searches the active language first, then the
English table, then returns the key itself. Missing keys are logged once in
English.

`TextOrFallback(key, fallback)` uses the same active-language and English-table
lookup, then returns the caller-provided fallback. This is used for migrated
data files that still keep English `name` and `description` fields.

`Format(key, values)` replaces `{token}` placeholders after lookup. Example:

```cpp
LocalizationManager::Get().Format("battle.log.attack", {
    { "actor", caster.GetName() },
    { "target", target->GetName() }
});
```

## Font Generation

DirectXTK `MakeSpriteFont` defaults to ASCII-only unless character regions are
provided. Vietnamese requires Latin-1, Latin Extended-A, Latin Extended-B, and
Vietnamese-specific precomposed glyph ranges.

The committed Vietnamese font was generated with:

```bat
tools\MakeSpriteFont.exe "Arial" assets\fonts\arial_16_vietnamese.spritefont /FontSize:16 /NoPremultiply /DefaultCharacter:0x3F /CharacterRegion:0x20-0x7E /CharacterRegion:0xC0-0xFF /CharacterRegion:0x100-0x17F /CharacterRegion:0x180-0x24F /CharacterRegion:0x1EA0-0x1EF9
```

The extra `0x180-0x24F` range is required for the horned O and horned U
codepoints used by Vietnamese text. French uses the same extended font because
its accented Latin glyphs are already covered by the Latin-1 and extended
regions.

## Menu Options

The title menu has an `Options` entry. Version 1 contains:

- `Language`
- `Back`

Changing the language cycles through `languages.json`, saves
`save/settings.json` immediately, and reloads the title font so the current
screen updates without a restart.

## Migrating Content

For new player-facing data, add localization keys next to stable ids:

```json
{
  "id": "potion_small",
  "nameKey": "item.potion_small.name",
  "descriptionKey": "item.potion_small.description",
  "name": "Small Potion",
  "description": "Restores 30 HP to one ally."
}
```

The English `name` and `description` fields remain as fallbacks while older
data is being migrated. Never store language settings inside save slots; slots
remain gameplay snapshots only.

## Verified Coverage

The current shipped language tables are:

- English (`en_us`)
- French (`fr_fr`)
- Vietnamese (`vi_vn`)

The first pass localizes:

- Title menu, options, save-slot headings, and slot feedback.
- Campfire main menu, save/load slot menus, hints, and feedback.
- Battle commands, skill labels/descriptions, item names/descriptions, and
  primary battle log messages.
- Inventory and lineup item/equipment text.
- Currency HUD labels and battle coin rewards.
- Overworld story area names and objectives.

Build verification:

```bat
.\build_src_static.bat 2>&1
```

The expected successful tail is:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```
