# Bug Postmortem: `damageTakenOccurMoment` Value Ignored

## 1. Symptoms
In the turn-based combat system, the property `damageTakenOccurMoment` within `data/skills/verso_attack.json` was meant to dictate the normalized progress of the attack animation at which damage is actually dealt to the target.

Despite changing the value in the JSON file to `0.1` and even `0.01` (meaning damage should trigger immediately at the start of the attack visualization), the actual damage in-game always applied at approximately `~0.8` (80% into the animation). The JSON setting was being completely ignored.

## 2. Investigation & Hypotheses

### Attempt 1: The `PlayClip` Guard Condition (Logic Bug)
- **Hypothesis:** When an animation finishes, its state is kept. If the same clip (`"attack-1"`) is played again on the next turn, maybe the visual system skips resetting the frame counters.
- **Investigation:** Looking at `WorldSpriteRenderer::PlayClip()`, there was a guard: 
  `if (mActiveClipName == clipName && mActiveClip != nullptr) return true;`
- **Action:** This guard skipped resetting the animation variables if the name matched, but failed to check if the clip had already finished (`mClipFinished`). This *was* a genuine bug that would cause immediate fallback states where `GetClipProgress() == 1.0f` instantly.
- **Fix:** Updated the guard to check `!mClipFinished` and `!mFrozen`.
- **Result:** While this fixed a problem for repeated attack calls, it **did not** solve the JSON value being ignored. The damage moment still behaved like `0.8f` on the very first turn.

### Attempt 2: Working Directory & File Loading (I/O Bug)
- **Hypothesis:** The relative file path `data/skills/verso_attack.json` might not be found due to Visual Studio debugger execution overriding the working directory. If `ifstream` fails to open the file, the parser falls back to the hardcoded default `0.8f`.
- **Investigation:** Injected debug logs directly into `JsonLoader::LoadSkillData` to print the loaded value (`out.damageTakenOccurMoment`) and confirm whether the file stream succeeded.
- **Result:** The console reported:
  `[JsonLoader] Loaded SkillData from 'data/skills/verso_attack.json'. damageMoment: 0.800000`
- **Conclusion:** The file *was* found and successfully loaded, but the parsed value was still falling back to `0.8f`.

### Attempt 3: The String Parsing Engine (Parsing Bug)
- **Hypothesis:** The string extraction logic `detail::ValueOf(src, "damageTakenOccurMoment")` might be failing to locate the key-value pair in the loaded string buffer.
- **Investigation:** Edited the `JsonLoader.h` log to print the raw, pre-parsed string extracted by `ValueOf`.
- **Result:** `(raw='')` — The extracted string was completely empty.
- **Conclusion:** Finding an empty string meant `std::string::find` failed to locate the literal `"damageTakenOccurMoment"` inside the loaded file data. But checking the file visually, the text was perfectly fine.

### Attempt 4: Hidden Byte Manipulation (The Root Cause)
- **Hypothesis:** The file's text encoding might not be standard ANSI/UTF-8. If it's UTF-16, C++'s standard `ifstream` (which expects 8-bit bytes) will read a series of zero-bytes (`\0`) between every character.
- **Investigation:** Dumped the raw bytes of `verso_attack.json` via PowerShell:
  `Get-Content data/skills/verso_attack.json -Encoding Byte`
- **Result:** The first two bytes returned were `255 254` (`FF FE` in hex) – this is the **Byte Order Mark (BOM) for UTF-16 Little Endian**. The subsequent bytes were interspersed with `0`.
- **The Catastrophe:** Because C++ strings (`std::string`) treat `\0` as a null-terminator, reading a UTF-16 file into a standard string abruptly cuts off the string at the very first character. The parsed string buffer was internally seen as garbage/empty. As a result, `ValueOf()` found nothing, and `ParseFloat()` safely applied the default config parameter `0.8f`!

## 3. Resolution

- **Encoding Fix:** Ran a batch operation via PowerShell to read and rewrite all affected `.json` files inside `data/skills/` into pure `UTF-8` with no BOM.
  ```powershell
  Get-Content data/skills/verso_attack.json | Set-Content -Encoding UTF8 data/skills/verso_attack.json
  ```
- **Renderer Patch:** Kept the previously discovered `PlayClip()` guard patch in place to guarantee that replaying the same completed animation on a subsequent turn behaves deterministically and resets its frame duration.

## 4. Technical Takeaways
1. **Never trust your text editor's visual output blindly.** When standard parsing fails on "obviously correct" text, verify the binary/hex raw representation to check for encoding mismatches (like UTF-16 LE).
2. **Standard C++ `<fstream>` and `<string>` default to UTF-8/ASCII.** They do not gracefully manage wide-character streams (UTF-16) out of the box unless explicitly using `std::wifstream` and `std::wstring`.
3. Provide descriptive fallbacks and warnings: the silent default parameter fallback behavior hid the fact that the JSON properties were fundamentally unreadable payload.
