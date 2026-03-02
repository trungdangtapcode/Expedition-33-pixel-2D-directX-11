# JsonLoader

**Files:** `src/Utils/JsonLoader.h`

---

## Purpose

`JsonLoader::LoadSpriteSheet()` reads a JSON animation descriptor from disk and
populates a `SpriteSheet` struct.  It is the only disk I/O that happens during
asset initialization — after it returns, the game never re-reads the file.

---

## Why a custom parser instead of a library?

The animation JSON schema is fixed and small: a handful of top-level integer
fields plus an `"animations"` array of simple objects.  A general-purpose JSON
library (nlohmann/json, rapidjson) would add a `vcpkg` dependency, a build step,
and hundreds of kilobytes of headers for a file that can be parsed in under 200
lines.

The rule: **use the simplest tool that handles exactly the schema you own**.
If the schema grows (nested objects, floats in more contexts, null values), swap
in a full library at that point.

---

## Usage

```cpp
SpriteSheet sheet;
if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet))
{
    LOG("[PlayState] ERROR — Failed to load verso.json.");
    return;
}
// sheet is now fully populated — pass to WorldSpriteRenderer::Initialize()
```

---

## How it works

`LoadSpriteSheet` reads the entire file into a single `std::string`, then
applies three text-search helpers:

| Helper | What it does |
|---|---|
| `detail::ValueOf(src, key)` | Finds `"key": <value>` and returns the raw text after `:` |
| `detail::ParseString(src, pos)` | Extracts the content between the next pair of `"` |
| `detail::Trim(s)` | Strips leading/trailing whitespace, CR, LF |

### Top-level fields

```cpp
sheet.name        = detail::ParseString(detail::ValueOf(src, "sprite_name"));
sheet.width       = std::stoi(detail::Trim(detail::ValueOf(src, "width")));
sheet.frameWidth  = std::stoi(detail::Trim(detail::ValueOf(src, "frame_width")));
// ... etc.
```

### Animation array

The parser finds the `"animations"` array by locating the `[` after the key,
then iterates `{` ... `}` blocks — each one is an `AnimationClip`:

```
"animations": [
  {  ← block start
    "name":       "idle",
    "num_frames": 14,
    ...
    "pivot":      [64, 128]
  }  ← block end
]
```

For each block:
1. Extract `"name"`, `"num_frames"`, `"frame_rate"`, `"loop"` with `ValueOf`.
2. Parse the `"pivot"` array: find `[`, read two integers separated by `,`.
3. Parse `"align"` string and map to the `SpriteAlign` enum.
4. Push the completed `AnimationClip` into `sheet.animations`.

### `SpriteAlign` string → enum mapping

| JSON string | Enum value |
|---|---|
| `"bottom-center"` | `SpriteAlign::BottomCenter` |
| `"top-left"` | `SpriteAlign::TopLeft` |
| `"middle-center"` | `SpriteAlign::MiddleCenter` |
| *(any other)* | `SpriteAlign::Unknown` (falls back to BottomCenter) |

---

## JSON schema the parser expects

```json
{
  "sprite_name":  "verso",
  "character":    "verso",
  "width":        1792,
  "height":       128,
  "frame_width":  128,
  "frame_height": 128,
  "animations": [
    {
      "name":       "idle",
      "num_frames": 14,
      "frame_rate": 8,
      "loop":       true,
      "pivot":      [64, 128],
      "align":      "bottom-center"
    }
  ]
}
```

Fields are matched by **key name** — order does not matter.

---

## Limitations

| Limitation | Impact |
|---|---|
| No float support for top-level fields | `frame_rate` must be an integer in JSON; `8.0` will parse as `8` |
| No nested objects | Cannot represent per-frame pivot overrides |
| No `null` values | Every expected field must be present |
| Brittle on malformed JSON | No validation beyond `std::stoi` / `std::stof` exceptions |

If any of these become blockers, replace `JsonLoader` with nlohmann/json from
vcpkg.  The only call site is `JsonLoader::LoadSpriteSheet()` — one function to
swap out.

---

## Common mistakes

| Mistake | Consequence | Fix |
|---|---|---|
| Calling `LoadSpriteSheet` every frame | Disk I/O stall, ~5–50 ms per call | Call once in `OnEnter()`; cache the `SpriteSheet` as a local variable |
| Wrong working directory | File not found, returns `false` | Run `bin\game.exe` from the workspace root so relative paths resolve |
| Editing JSON key names without updating the parser | Field silently reads as `0` or `""` | Parser matches exact key strings — schema and parser must stay in sync |
