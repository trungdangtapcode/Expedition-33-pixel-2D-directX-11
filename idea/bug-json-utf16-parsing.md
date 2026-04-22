# Bug Report: JSON UTF-16 LE Encoding Breaking Data-Driven Parameters

## Symptom
There was a severe visual delay during the Skeleton enemy's attack sequence. The Skeleton would execute its attack, fully return to its original position, and only afterwards would the player visually trigger the damage response. Attempting to fix this by modifying `data/skills/skeleton_attack.json` to set `"damageTakenOccurMoment": 0` had absolutely no effect. The engine was seemingly ignoring the data-driven configuration completely.

## Investigation
1. **Event Sync Fix**: Initially suspected that the UI Health Bar was only resolving at the end of `BattleManager`'s action queue. Modified `Combatant::TakeDamage()` to broadcast the `hp_changed` event instantly. The delay still persisted.
2. **Runtime Verification**: Injected heavy logging into `JsonLoader.h` and `EnemyCombatant.cpp` to verify what configuration values were actually arriving in the C++ logic.
3. **The Clue**: Log output conclusively showed: `[JsonLoader] Loaded SkillData from 'data/skills/skeleton_attack.json' ... mMoment=0.800000`. The engine was ignoring the `0` from the JSON and falling back to its hardcoded default, `0.8f`.
4. **Byte-level Analysis**: Read the raw bytes of the `.json` file using Python: `b'\xff\xfe{\x00 \x00"\x00m\x00o\x00v\x00...`. The `\xff\xfe` prefix is the Byte Order Mark (BOM) for **UTF-16 Little Endian**.

## Root Cause
The codebase's custom `JsonLoader` utility uses `std::string` and string operations like `find()` to parse JSON keys. Because UTF-16 LE encodes standard ASCII characters with interleaved null bytes (e.g., `"d"` becomes `0x64 0x00`), `std::string::find("\"damageTakenOccurMoment\"")` silently failed. It couldn't find the key, so it safely defaulted to `0.8f`, causing the attack damage to always trigger near the end of the animation.

## Resolution
1. Converted `skeleton_attack.json` (and confirmed others) back to standard **UTF-8** encoding without BOM.
2. The `JsonLoader` now correctly parses the string, recognizes `"damageTakenOccurMoment": 0`, and passes it to `AnimDamageAction`, successfully syncing the attack instantly.
3. Added a warning `detail::WarnIfUTF16(src, path);` directly into the parsing logic to prevent future silent fallback failures.

## Lessons Learned
- **Encoding hygiene**: Always enforce UTF-8 formatting for text and JSON data assets. Tools like Windows Notepad can sometimes save files as "Unicode" (which actually means UTF-16 LE), silently breaking parsers.
- **Data-Driven validation**: When an engine ignores a data-driven tweak, the first step should be to log the exact float/int value immediately as it leaves the parser to guarantee the configuration was actually ingested. 
- Avoid relying entirely on visual intuition to debug timing properties without verifying the numeric data representation in memory first.